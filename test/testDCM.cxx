#include "config/Configuration.h"
#include "dal/Partition.h"
#include "DFdal/DFParameters.h"

#include "msg/Port.h"
#include "msgconf/MessageConfiguration.h"
#include "msginput/MessageHeader.h"

#include "cmdl/cmdargs.h"

#include "dcmessages/LVL1Result.h"
#include "RunController/ConfigurationBridge.h"
#include "RunController/Controllable.h"
#include "RunController/ItemCtrl.h"

#include "queues/ProtectedQueue.h"

#include "ipc/core.h"
#include "ers/ers.h"
#include "is/infoT.h"
#include "is/infodictionary.h"

#include "asyncmsg/NameService.h"

#include "DCMMessages.h"
#include "HLTSVSession.h"

#include <stdlib.h>
#include <atomic>
#include <random>

// Fixme: Let's make these config. params
#define NUM_DCM_CORES	8
#define REJECT_RATE     0.95
// max sleep times in ms
#define PROCESS_TIME    60
#define BUILD_TIME      4000

// *******************
class DCMActivity : public daq::rc::Controllable {
public:
  DCMActivity(std::string & name);
  ~DCMActivity();

  virtual void initialize(std::string & ) {};
  virtual void configure(std::string & );
  virtual void unconfigure(std::string & );
  virtual void connect(std::string & );
  virtual void disconnect(std::string & );
  virtual void stopL2SV(std::string & );
  virtual void prepareForRun(std::string & );
  virtual void probe(std::string& );

  void execute(unsigned worker_id);

private:
  MessageConfiguration m_msgconf;
  daq::asyncmsg::NameService *m_testns;
  std::shared_ptr<hltsv::HLTSVSession> m_session;

  boost::asio::io_service m_dcm_io_service;
  std::unique_ptr<boost::asio::io_service::work> m_work;
  boost::thread m_service_thread;
  std::vector<std::unique_ptr<boost::thread>> m_work_threads;

  bool m_running;
  ProtectedQueue<dcmessages::LVL1Result*> m_queue;
  tbb::atomic<uint32_t>            m_outstanding;
  uint32_t                         m_cores;
  
  uint32_t                         m_l2_processing;
  uint32_t                         m_l2_accept;
  uint32_t                         m_event_building;
  uint32_t                         m_event_filter;

  std::unique_ptr<ISInfoDictionary> m_dict;
  std::atomic<uint64_t>             m_events;
};

// *******************



DCMActivity::DCMActivity(std::string &name)
    : daq::rc::Controllable(name),
      m_running(false)
{
}


DCMActivity::~DCMActivity()
{
}


void DCMActivity::configure(std::string & )
{
  Configuration *conf = daq::rc::ConfigurationBridge::instance()->getConfiguration();

  const daq::core::Partition *partition = daq::rc::ConfigurationBridge::instance()->getPartition();
  IPCPartition part(partition->UID());
  const daq::df::DFParameters *dfparams = conf->cast<daq::df::DFParameters>(partition->get_DataFlowParameters());

  m_testns = new daq::asyncmsg::NameService(part, dfparams->get_DefaultDataNetworks());
  m_dict.reset(new ISInfoDictionary(part));
}


void DCMActivity::unconfigure(std::string & )
{
  m_msgconf.unconfigure();
  delete m_testns;
}


void DCMActivity::connect(std::string &)
{  
  // Read the HLTSV port using the name Service
  std::vector<boost::asio::ip::tcp::endpoint> hltsv_eps = m_testns->lookup("HLTSV");

  // There can be only one!
  assert(hltsv_eps.size() == 1);
  ERS_LOG("Found: " << hltsv_eps.size() << " endpoints");
  ERS_LOG("Found the port: " << hltsv_eps[0].port() );

  // Start ASIO Client
  ERS_LOG(" *** Start io_service ***");
  auto func = [&] () {
    ERS_LOG(" *** Run io_service ***");
    m_dcm_io_service.run(); 
    ERS_LOG(" *** io_service End ***");
  };
  // at connect, we should not have a service thread running yet (i.e. equals Not-a-Thread)
  assert(m_service_thread.get_id() == boost::thread::id());
  m_work.reset( new boost::asio::io_service::work(m_dcm_io_service) );
  m_service_thread = boost::thread(func); 
  ERS_LOG(" *** installed service thread *** ");

  // create the session to talk to the HLTSV    
  m_session = std::make_shared<hltsv::HLTSVSession>(m_dcm_io_service);
  ERS_LOG(" *** created HLTSVSession *** ");
  m_session->asyncOpen("HLTSV", hltsv_eps[0]);
}

void DCMActivity::disconnect(std::string & )
{
  ERS_LOG("DCMActivity::disconnect(): close HLTSV connection");
  // disconnect from HLTSV
  m_session->close();
  ERS_LOG("DCMActivity::disconnect(): join to service thread");

  // allow io_service to finish what it's doing by explictly destroying the work object
  m_work.reset();
  m_service_thread.join();
}

void DCMActivity::stopL2SV(std::string & )
{
  // stop the run loop and wake any threads waiting on HLTSV
  m_running = false;
  m_session->abort_queue();

  // join to the work threads and delete them
  for (auto & worker : m_work_threads)
  {
    worker->join();
  }
  m_work_threads.clear();
}

void DCMActivity::prepareForRun(std::string & )
{
  ERS_LOG(" *** enter DCMActivity:prepareForRun *** ");
  m_running = true;

  // Announce initial state to HLTSV (assumes connection opened succesfully!)
  std::vector<uint32_t> l1ids;
  m_session->send_update(NUM_DCM_CORES, l1ids);

  // listen for first update from HLTSV
  m_session->asyncReceive();

  // spin off worker threads to run execute()
  for (unsigned worker_id = 0; worker_id < NUM_DCM_CORES; ++worker_id)
  {
    std::unique_ptr<boost::thread> worker(new boost::thread(
		std::bind(&DCMActivity::execute, this, worker_id)
		));
    m_work_threads.push_back( std::move(worker) );
  }

  ERS_LOG(" *** end of DCMActivity:prepareForRun *** ");

}

void DCMActivity::probe(std::string& )
{
    ISInfoUnsignedLong value;
    value.setValue(m_events);
    m_dict->checkin( "DF." + getName(), value);
}


// main run loop for the DCM worker cores
void DCMActivity::execute(unsigned worker_id)
{
  ERS_LOG(" *** Worker #" << worker_id << ": Entering DCMActivty::execute() *** ");

  unsigned l1id;
  std::vector<uint32_t> l1id_list;  // needed for handing off to HLTSVSession
  l1id_list.push_back(0);
  while (m_running)
  {
    try {
      l1id = m_session->get_next_assignment();
    }
    catch (tbb::user_abort &e) {
      // queue was aborted by StopL2SV. we're done here!
      break;
    }

    m_events++;

    ERS_DEBUG(1,"Worker #" << worker_id << ": got assigned L1ID " << l1id);

    // recieved a new L1ID, 'process' for a random time
    usleep( (PROCESS_TIME * 1000) * rand() / float(RAND_MAX) );

    // finished 'processing', decide whether to reject and send back the L1ID either way
    bool do_build = (rand() / float(RAND_MAX)) > REJECT_RATE;

    l1id_list[0] = l1id;
    uint32_t num_cores = do_build ? 0 : 1;

    m_session->send_update(num_cores, l1id_list);

    if (do_build)
    {
      // simulate extra processing; sleep some more then request a new work unit
      usleep( (BUILD_TIME * 1000) * rand() / float(RAND_MAX) );
      m_session->send_update(1, std::vector<uint32_t>());
    }
  }

  ERS_LOG(" *** Exiting DCMActivity::execute() *** ");
}

int main(int argc, char *argv[])
{

  try{
    IPCCore::init(argc,argv);
  } catch(daq::ipc::Exception& ex) {
    ers::fatal(ex);
    exit(EXIT_FAILURE);
  }

  std::string name;
  std::string parent;
  bool interactive;
  
  CmdArgStr     app('N',"name", "name", "application name", CmdArg::isOPT);
  CmdArgStr     uniqueId('n',"uniquename", "uniquename", "unique application id", CmdArg::isREQ);
  CmdArgBool    iMode('i',"iMode", "turn on interactive mode", CmdArg::isOPT);
  CmdArgStr     segname('s',"segname", "segname", "segment name", CmdArg::isOPT);
  CmdArgStr     parentname('P',"parentname", "parentname", "parent name", CmdArg::isREQ);

  
  segname = "";
  parentname = "";
  
  CmdLine       cmd(*argv, &app, &uniqueId, &iMode, &segname, &parentname, NULL);
  CmdArgvIter   argv_iter(--argc, (const char * const *) ++argv);

  
  unsigned int status = cmd.parse(argv_iter);
  if (status) {
    cmd.error() << argv[0] << ": parsing errors occurred!" << std::endl ;
    exit(EXIT_FAILURE);
  }
  name = uniqueId;
  interactive = iMode;
  parent = parentname;
  
  daq::rc::ItemCtrl control(new DCMActivity(name), interactive, parent);

  control.run();

}

