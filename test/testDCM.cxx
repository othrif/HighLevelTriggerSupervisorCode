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

#include "asyncmsg/NameService.h"

#include "DCMMessages.h"
#include "HLTSVSession.h"


// *******************
class DCMActivity : public daq::rc::Controllable {
public:
  DCMActivity(std::string & name);
  ~DCMActivity();

  virtual void initialize(std::string & ) {};
  virtual void configure(std::string & );
  virtual void unconfigure(std::string & );
  virtual void connect(std::string & );
  virtual void stopL2SV(std::string & );
  virtual void prepareForRun(std::string & );

  //void execute();

private:
  MessageConfiguration m_msgconf;
  daq::asyncmsg::NameService *m_testns;
  std::shared_ptr<hltsv::HLTSVSession> m_session;

  boost::asio::io_service m_dcm_io_service;
  boost::asio::io_service::work m_work;

  bool m_running;
  ProtectedQueue<dcmessages::LVL1Result*> m_queue;
  std::vector<boost::thread*>      m_handlers;
  tbb::atomic<uint32_t>            m_outstanding;
  uint32_t                         m_cores;
  
  uint32_t                         m_l2_processing;
  uint32_t                         m_l2_accept;
  uint32_t                         m_event_building;
  uint32_t                         m_event_filter;
};

// *******************



DCMActivity::DCMActivity(std::string &name)
    : daq::rc::Controllable(name),
      m_work(m_dcm_io_service), m_running(false)
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
  ERS_LOG("Found: " << hltsv_eps.size() << " endpoints");
  ERS_LOG("Found the port: " << hltsv_eps[0].port() );

  // Start ASIO Client
  ERS_LOG(" *** Start io_service ***");
  auto func = [&] () {
    ERS_LOG(" *** Run io_service ***");
    m_dcm_io_service.run(); 
    ERS_LOG(" *** io_service End ***");
  };
  boost::thread service_thread(func); 
  ERS_LOG(" *** installed service thread *** ");

  // create the session to talk to the HLTSV    
  m_session = std::make_shared<hltsv::HLTSVSession>(m_dcm_io_service);
  ERS_LOG(" *** created HLTSVSession *** ");
  m_session->asyncOpen("HLTSV", hltsv_eps[0]);

}


void DCMActivity::stopL2SV(std::string & )
{
  m_running = false;
}


void DCMActivity::prepareForRun(std::string & )
{
  ERS_LOG(" *** enter prepareForRun *** ");
  m_running = true;


  auto func = [&] () {
    ERS_LOG(" *** Run thread for running ***");
    std::vector<uint32_t> l1ids;
    uint32_t reqRoIs = 3;
    std::unique_ptr<const hltsv::RequestMessage> test_msg(new hltsv::RequestMessage(reqRoIs,l1ids));
    
    while(m_running) {
      
      m_session->asyncSend(std::move(test_msg));
      ERS_LOG(" *** DCM::execute onSend *** ");
      break;
    }
    sleep(20);
    ERS_LOG(" *** End of running thread ***");
  };
  boost::thread execute_thread(func); 
}
  
 
// void DCMActivity::execute()
// {
//   ERS_LOG(" *** enter execute() *** ");

//   std::vector<uint32_t> l1ids;
//   uint32_t reqRoIs = 3;
//   std::unique_ptr<const hltsv::RequestMessage> test_msg(new hltsv::RequestMessage(reqRoIs,l1ids));
    
//   while(m_running) {
      
//     m_session->asyncSend(std::move(test_msg));
//     ERS_LOG(" *** DCM::execute onSend *** ");
//     break;
//   }
//   sleep(20);

// }


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

