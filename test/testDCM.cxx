
#include "config/Configuration.h"
#include "dal/Partition.h"
#include "DFdal/DFParameters.h"
#include "DFdal/HLTSV_DCMTest.h"

#include "cmdl/cmdargs.h"

#include "RunControl/Common/OnlineServices.h"
#include "RunControl/Common/Controllable.h"
#include "RunControl/Common/CmdLineParser.h"
#include "RunControl/ItemCtrl/ItemCtrl.h"

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
#include <memory>

// *******************
class DCMActivity : public daq::rc::Controllable {
public:
  DCMActivity();
  ~DCMActivity() noexcept;

  virtual void configure(const daq::rc::TransitionCmd&  ) override;
  virtual void unconfigure(const daq::rc::TransitionCmd&  ) override;
  virtual void connect(const daq::rc::TransitionCmd&  ) override;
  virtual void disconnect(const daq::rc::TransitionCmd&  ) override;
  virtual void stopDC(const daq::rc::TransitionCmd&  ) override;
  virtual void prepareForRun(const daq::rc::TransitionCmd&  ) override;
  virtual void publish() override;

  void execute(unsigned worker_id);

private:
  daq::asyncmsg::NameService *m_testns;
  std::shared_ptr<hltsv::HLTSVSession> m_session;

  boost::asio::io_service m_dcm_io_service;
  std::unique_ptr<boost::asio::io_service::work> m_work;
  boost::thread m_service_thread;
  std::vector<std::unique_ptr<boost::thread>> m_work_threads;

  bool m_running;
  tbb::atomic<uint32_t>            m_outstanding;
  uint32_t                         m_cores;
  
  uint32_t                         m_l2_processing;
  uint32_t                         m_l2_accept;
  uint32_t                         m_event_building;
  uint32_t                         m_event_filter;
  uint32_t                         m_fake_timeout;

  std::unique_ptr<ISInfoDictionary> m_dict;
  std::atomic<uint64_t>             m_events;
};

// *******************



DCMActivity::DCMActivity()
    : daq::rc::Controllable(),
      m_running(false),
      m_cores(8),
      m_l2_processing(40),
      m_l2_accept(5),
      m_event_building(40),
      m_fake_timeout(0)
{
}


DCMActivity::~DCMActivity() noexcept
{
}


void DCMActivity::configure(const daq::rc::TransitionCmd&  )
{
  Configuration& conf = daq::rc::OnlineServices::instance().getConfiguration();

  const daq::core::Partition& partition = daq::rc::OnlineServices::instance().getPartition();
  IPCPartition part = daq::rc::OnlineServices::instance().getIPCPartition();

  const daq::df::DFParameters *dfparams = conf.cast<daq::df::DFParameters>(partition.get_DataFlowParameters());
  m_testns = new daq::asyncmsg::NameService(part, dfparams->get_DefaultDataNetworks());

  const daq::df::HLTSV_DCMTest* self = conf.cast<daq::df::HLTSV_DCMTest>(&daq::rc::OnlineServices::instance().getApplication());

  m_cores = self->get_NumberOfCores();
  m_l2_processing = self->get_L2ProcessingTime();
  m_l2_accept      = self->get_L2Acceptance();
  m_event_building = self->get_EventBuildingTime();
  m_event_filter   = self->get_EFProcessingTime();
  m_fake_timeout   = self->get_FakeTimeout();

  m_dict.reset(new ISInfoDictionary(part));
}


void DCMActivity::unconfigure(const daq::rc::TransitionCmd&  )
{
  delete m_testns;
}


void DCMActivity::connect(const daq::rc::TransitionCmd& )
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
  m_session->asyncOpen(daq::rc::OnlineServices::instance().applicationName(), hltsv_eps[0]);

  while(m_session->state() != daq::asyncmsg::Session::State::OPEN) {
      usleep(10000);
  }
}

void DCMActivity::disconnect(const daq::rc::TransitionCmd&  )
{
  ERS_LOG("DCMActivity::disconnect(): close HLTSV connection");
  // disconnect from HLTSV
  m_session->asyncClose();
  while(m_session->state() != daq::asyncmsg::Session::State::CLOSED) {
      usleep(10000);
  }

  ERS_LOG("DCMActivity::disconnect(): join to service thread");
  // allow io_service to finish what it's doing by explictly destroying the work object
  m_work.reset();
  m_service_thread.join();
}

void DCMActivity::stopDC(const daq::rc::TransitionCmd&  )
{
  // stop the run loop and wake any threads waiting on HLTSV
  m_running = false;
  m_session->abort_assignment_queue();

  // join to the work threads and delete them
  for (auto & worker : m_work_threads)
  {
    worker->join();
  }
  m_work_threads.clear();
}

void DCMActivity::prepareForRun(const daq::rc::TransitionCmd&  )
{
  ERS_LOG(" *** enter DCMActivity:prepareForRun *** ");
  m_running = true;

  // Announce initial state to HLTSV (assumes connection opened succesfully!)
  std::vector<uint32_t> l1ids;
  m_session->send_update(m_cores, l1ids);

  // listen for first update from HLTSV
  m_session->asyncReceive();

  // spin off worker threads to run execute()
  for (unsigned worker_id = 0; worker_id < m_cores; ++worker_id)
  {
    std::unique_ptr<boost::thread> worker(new boost::thread(
		std::bind(&DCMActivity::execute, this, worker_id)
		));
    m_work_threads.push_back( std::move(worker) );
  }

  ERS_LOG(" *** end of DCMActivity:prepareForRun *** ");

}

void DCMActivity::publish()
{
    ISInfoUnsignedLong value;
    value.setValue(m_events);
    m_dict->checkin( "DF." + daq::rc::OnlineServices::instance().applicationName(), value);
}


// main run loop for the DCM worker cores
void DCMActivity::execute(unsigned worker_id)
{
  ERS_LOG(" *** Worker #" << worker_id << ": Entering DCMActivty::execute() *** ");

  std::random_device rd;
  std::mt19937 engine(rd());

  std::normal_distribution<> l2_distribution(m_l2_processing, 1);
  std::normal_distribution<> eb_distribution(m_event_building, 1);
  std::normal_distribution<> ef_distribution(m_event_filter, 1);

  unsigned int l1id;
  bool do_build;
  std::vector<uint32_t> l1id_list(1);  // needed for handing off to HLTSVSession

  while (m_running)
  {
    // block until a new L1ID is available.
    try {
        std::tie(do_build, l1id) = m_session->get_next_assignment();
        
        if(!do_build) {
            // else, randomly decide whether we'll build
            do_build = (rand() % 100) <  m_l2_accept;
        }
    } catch (tbb::user_abort &e) {
        if (m_running) {
            // queue was interrupted, probably a new force build came in
            continue;
        } else {
            // queue was aborted by StopDC. we're done here!
            break;
        }
    }

    m_events++;

    ERS_DEBUG(1,"Worker #" << worker_id << ": got assigned L1ID " << l1id);

    // Temporary hack to test HLTSV timeout handling.
    // Every 1e6 events, sleep for 15s (unless rejected).
    // After, DCM will try to clear the stale l1id and request a new work unit
    if (m_fake_timeout > 0) {
      if (!do_build && (l1id % m_fake_timeout == 0)) {
        ERS_LOG("faking timeout! L1ID = " << l1id);
        sleep(15);
        l1id_list[0] = l1id;
        m_session->send_update(1, l1id_list);
        continue;
      }
    }

    // recieved a new L1ID, 'process' for a random time

    usleep(l2_distribution(engine) * 1000);

    // finished 'processing', send back the L1ID immediately
    // number of cores depends on whether we're going to build stage
    l1id_list[0] = l1id;
    uint32_t num_cores = do_build ? 0 : 1;
    m_session->send_update(num_cores, l1id_list);

    if (do_build)
    {
      // simulate extra processing; sleep some more then request a new work unit
      usleep(eb_distribution(engine) * 1000);

      // EF
      usleep( ef_distribution(engine) * 1000);

      m_session->send_update(1, std::vector<uint32_t>());

    }
  }

  ERS_LOG(" *** Exiting DCMActivity::execute() *** ");
}

int main(int argc, char *argv[])
{

  try{
    IPCCore::init(argc,argv);
    daq::rc::CmdLineParser cmdline(argc, argv);
    daq::rc::ItemCtrl control(cmdline, std::make_shared<DCMActivity>());
    
    std::random_device rd;
    srand(rd());
    control.init(); 
    control.run();
    
  } catch(ers::Issue& ex) {
      ers::fatal(ex);
      exit(EXIT_FAILURE);
  }

}

