
#include "Activity.h"

#include <stdlib.h>

#include "dynlibs/DynamicLibrary.h"

#include "config/Configuration.h"

#include "dal/Partition.h"
#include "dal/MasterTrigger.h"

#include "DFdal/DFParameters.h"
#include "DFdal/DataFile.h"

#include "rc/RunParamsNamed.h"

#include "L1Source.h"
#include "LVL1Result.h"
#include "ROSClear.h"
#include "UnicastROSClear.h"
#include "MulticastROSClear.h"
#include "EventScheduler.h"

#include "DFdal/HLTSVApplication.h"
#include "DFdal/RoIBPlugin.h"

#include "RunControl/Common/OnlineServices.h"

#include "monsvc/MonitoringService.h"
#include "monsvc/FilePublisher.h"

#include "HLTSV.h"

#include "Issues.h"
#include "ers/ers.h"

#include "asyncmsg/NameService.h"

#include <is/info.h>

#include <algorithm>
#include <chrono>
#include <ctime>

namespace hltsv {

  const size_t Num_Assign = 12;

  Activity::Activity()
    : daq::rc::Controllable(), 
      m_l1source(nullptr),
      m_network(false),
      m_running(false),
      m_triggering(false),
      m_cmdReceiver(0),
      m_triggerHoldCounter(0),
      m_masterTrigger(false)
  {
  }

  Activity::~Activity() noexcept
  {
  }
  
  void Activity::configure(const daq::rc::TransitionCmd& )
  {

    ERS_LOG(" *** Start of Activity::configure() ***");

    Configuration& conf = daq::rc::OnlineServices::instance().getConfiguration();

    const daq::df::HLTSVApplication* self = conf.cast<daq::df::HLTSVApplication>(&daq::rc::OnlineServices::instance().getApplication());

    const daq::core::Partition& partition = daq::rc::OnlineServices::instance().getPartition();
    const IPCPartition          part(daq::rc::OnlineServices::instance().getIPCPartition());

    const daq::df::DFParameters *dfparams = conf.cast<daq::df::DFParameters>(partition.get_DataFlowParameters());

    m_event_delay = self->get_EventDelay();

    // Load L1 Source
    std::vector<std::string> file_names;
    const std::vector<const daq::df::DataFile*>& dataFiles = dfparams->get_UsesDataFiles();
    std::transform(dataFiles.begin(), dataFiles.end(), file_names.begin(), [](const daq::df::DataFile* df) { return df->get_FileName(); });

    const daq::df::RoIBPlugin *source = self->get_RoIBInput();

    try {
        for(const auto& lib_name : source->get_Libraries()) {
          m_l1source_libs.emplace_back(new DynamicLibrary(lib_name));
        }
        
        if(m_l1source_libs.size() == 0) {
          throw ConfigFailed(ERS_HERE, "No libraries specified for RoIBPlugin");
        }

        if(L1Source::creator_t make =  m_l1source_libs.back().get()->function<L1Source::creator_t>("create_source")) {
            m_l1source = make(&conf, source, file_names);
            m_l1source->preset();
        } else {
            // fatal
        }
    } catch (ers::Issue& ex) {
      ers::fatal(ex);
      return;
    }


    // Conditions for which the HLTSV should be Master Trigger
    const daq::core::MasterTrigger* masterholder = 0;
    masterholder = partition.get_MasterTrigger();
    const daq::core::RunControlApplicationBase * master = 0;
    const daq::df::HLTSVApplication * hltsvApp = 0;
    if (masterholder) {
      master = masterholder->get_Controller();
      hltsvApp = conf.cast<daq::df::HLTSVApplication>(master);
      if(source->get_IsMasterTrigger()) {
        // Check if OKS is set correctly
	if(hltsvApp) {
	  // HLTSV is master trigger
	  m_masterTrigger = true;
	  m_cmdReceiver = new daq::trigger::CommandedTrigger(part, daq::rc::OnlineServices::instance().applicationName(),this);
	} else {
	  std::stringstream issue_txt;
	  issue_txt << "HLTSV is not the master trigger, even if it is set to " 
		    << source->UID() << " mode. The master trigger is: " 
		    << masterholder->UID();
	  std::string tmp = issue_txt.str();
	  hltsv::MasterTriggerIssue fatal_i(ERS_HERE, tmp.c_str());
	  ers::fatal(fatal_i);
	}   
      } else {
	if(hltsvApp) {
	  std::stringstream issue_txt;
	  issue_txt << "HLTSV is set as the master trigger, but the source type is: " 
		    <<  source->UID();
	  std::string tmp = issue_txt.str();
	  hltsv::MasterTriggerIssue warn_i(ERS_HERE, tmp.c_str());
	  ers::warning(warn_i);
	}
      }
    } else {
      std::string issue_txt = "Master Trigger not defined in the partition";
      hltsv::MasterTriggerIssue warn2_i(ERS_HERE, issue_txt.c_str());
      ers::warning(warn2_i);
    }

    m_publisher.reset(new monsvc::PublishingController(part,daq::rc::OnlineServices::instance().applicationName()));
    m_publisher->add_configuration_rules(conf);

    m_io_services.reset(new std::vector<boost::asio::io_service>(self->get_NumberOfAssignThreads()));

    m_work.reset(new std::vector<boost::asio::io_service::work>());
    for(auto& svc : *m_io_services) {
        (*m_work).emplace_back(svc);
    }

    m_ros_work.reset(new boost::asio::io_service::work(m_ros_io_service));

    // Initialize  HLTSV_NameService
    std::vector<std::string> data_networks = dfparams->get_DefaultDataNetworks();
    ERS_LOG("number of Data Networks  found: " << data_networks.size());
    ERS_LOG("First Data Network : " << data_networks[0]);
    daq::asyncmsg::NameService HLTSV_NameService(part, data_networks);

    // Initialize ROS clear implementation
    if(dfparams->get_MulticastAddress().empty()) {
        // use TCP
        m_ros_clear = std::make_shared<UnicastROSClear>(self->get_ClearGrouping(), m_ros_io_service, HLTSV_NameService);
    } else {

        // address is format  <Multicast-IP-Adress>/<OutgoingInterface>

        auto mc = dfparams->get_MulticastAddress();
        auto n = mc.find('/');
        auto mcast = mc.substr(0, n);
        auto outgoing = mc.substr(n + 1);

        ERS_LOG("Configuring for multicast: " << mcast << '/' << outgoing);

        m_ros_clear = std::make_shared<MulticastROSClear>(self->get_ClearGrouping(), m_ros_io_service, mcast, outgoing);
    }
    
    m_network = true;

    ERS_LOG(" *** Start HLTSVServer ***");
    m_event_sched = std::make_shared<EventScheduler>();
    m_myServer = std::make_shared<HLTSVServer> (*m_io_services, m_event_sched, m_ros_clear, self->get_Timeout());

    m_myServer->listen(daq::rc::OnlineServices::instance().applicationName());

    boost::asio::ip::tcp::endpoint my_endpoint = m_myServer->localEndpoint();
    ERS_LOG("Local endpoint: " << my_endpoint);

    // Publish port in IS for DCM
    HLTSV_NameService.publish(daq::rc::OnlineServices::instance().applicationName(), my_endpoint.port());

    //  HLTSVServer::start() calls Server::asyncAccept() which calls HLTSVServer::onAccept
    m_myServer->start();

    for(unsigned int i = 0; i < self->get_NumberOfAssignThreads(); i++) {
        m_io_threads.push_back(std::thread(&Activity::io_thread, this, std::ref((*m_io_services)[i])));
    }

    m_io_threads.push_back(std::thread(&Activity::io_thread, this, std::ref(m_ros_io_service)));

    return;
  }

  void Activity::connect(const daq::rc::TransitionCmd& )
  {
    // ROS_Clear with TCP will establish the connection
    m_ros_clear->connect();
    
    m_publisher->start_publishing();
    return;
  }

  void Activity::prepareForRun(const daq::rc::TransitionCmd& )
  {
    const IPCPartition  partition(daq::rc::OnlineServices::instance().getIPCPartition());

    RunParamsNamed runparams(partition, "RunParams.SOR_RunParams");
    runparams.checkout();

    m_l1source->reset(runparams.run_number);

    m_running = true;
    m_triggering = true;

    m_l1_thread = std::thread(&Activity::l1_thread, this);
    
    return; 
  }
    
  void Activity::stopDC(const daq::rc::TransitionCmd& )
  {

    m_event_sched->push_events();

    m_triggering = false;
    m_running = false;


    m_l1_thread.join();
    
    return;
  }

  void Activity::stopRecording(const daq::rc::TransitionCmd& )
  {
    m_ros_clear->flush();
  }


  void Activity::unconfigure(const daq::rc::TransitionCmd& )
  {

    ERS_LOG("HLTSV is UNCONFIGURING");

    m_myServer->stop();

    m_network = false;

    m_work.reset();
    m_ros_work.reset();

    for(auto& thr : m_io_threads) {
        thr.join();
    }
    m_io_threads.clear();

    m_event_sched.reset();
    m_ros_clear.reset();
    m_myServer.reset();

    if(m_l1source) {
      delete m_l1source;
      m_l1source = 0;
    } else {
      ERS_LOG("m_l1source = 0");
    }

    m_l1source_libs.clear();

    for(auto& svc : *m_io_services) {
        svc.reset();
    }
    m_ros_io_service.reset();

    if(m_masterTrigger && m_cmdReceiver) {
      m_cmdReceiver->shutdown(); // shutdown() will delete m_cmdReceiver
      m_cmdReceiver = 0;
    }

    return;
  }

  void Activity::disconnect(const daq::rc::TransitionCmd&  )
  {
      m_publisher->stop_publishing();
      return;
  }


  void Activity::l1_thread()
  {
    ERS_LOG("Starting l1 thread");

    using namespace std::chrono;

    const uint16_t target_delay = (m_event_delay & 0xFFFF0000) >> 16; // MSbits (first 16)
    unsigned long ns_target_delay = target_delay*1e3; // target delay in ns  
    nanoseconds t_target_delay = nanoseconds(ns_target_delay); // target_delay in count ticks

    const uint16_t granularity = m_event_delay & 0x0000FFFF; //LSbits (last 16)
    unsigned long us_granularity = granularity*1; // granularity in us
    microseconds t_granularity = microseconds(us_granularity); // granularity in count ticks

    uint32_t trigger_count = 0; // count when you should sleep
    steady_clock::time_point t_last; // time since we started couting trigger_count

    // # of events to skip between clock-checking
    uint32_t correction_rate = t_granularity.count()/ ( (target_delay>0) ? target_delay : 1);
    if(correction_rate == 0) correction_rate = 1; // 0 events does not make sense

    // maximum number of trigger counts to average over.
    // if this is too large, high frequency fluctuations may not be compensated.
    // if it's too small, you won't have good average performance.
    const uint32_t MAXIMUM_TRIGGER_COUNT = 1e9; // 1e9 / 100kHz = 2.8 hrs.

    while(m_running) {
      if(m_triggering) {

    if ( (trigger_count == 0) || (trigger_count > MAXIMUM_TRIGGER_COUNT) ) {
      t_last = steady_clock::now();
      trigger_count = 0;
	}
	
	if (trigger_count % correction_rate == 0) {
	  steady_clock::time_point now = steady_clock::now();

	  duration<double> t_elapsed = duration_cast<duration<double>>(now - t_last);
	  auto t_delta = trigger_count * t_target_delay - t_elapsed;
	  
	  std::this_thread::sleep_for(t_delta);
	  
	}
	trigger_count++;
      
	std::shared_ptr<LVL1Result> result(m_l1source->getResult());
	if(result) {
	  m_event_sched->schedule_event(result);
	  ERS_DEBUG(1,"L1ID #" << result->l1_id() << "  has been scheduled");
	}
      } else {
	usleep(100000);
      }
    }
    ERS_LOG("Finishing l1 thread");

  }



  void Activity::io_thread(boost::asio::io_service& service)
  {
      while(m_network) {
          ERS_LOG(" *** Start io_service ***");
          service.run(); 
          ERS_LOG(" *** io_service End ***");
      };
  }


  /**
   *     MasterTrigger interface
   **/
    
  uint32_t Activity::hold()
  {
    m_triggerHoldCounter += 1;
    if(m_masterTrigger)  m_triggering = false;
    return 0;
  }
  
  void Activity::resume()
  {
    if (m_triggerHoldCounter > 0) {
      m_triggerHoldCounter -= 1;
    }
    
    if (m_triggerHoldCounter == 0 && m_masterTrigger) {
      m_triggering = true;
    } 
  }
  
  void Activity::setL1Prescales(uint32_t )
  {
    std::string issue_txt = "HLTSV cannot set L1 Prescale";
    hltsv::MasterTriggerIssue warn(ERS_HERE, issue_txt.c_str());
    ers::warning(warn);
  }
  
  void Activity::setHLTPrescales(uint32_t , uint32_t lb)
  {
    m_l1source->setHLTCounter(lb);
  }
  
  void Activity::increaseLumiBlock(uint32_t lb)
  {
  }

  void Activity::setLumiBlockInterval(uint32_t seconds)
  {
  }

  void Activity::setMinLumiBlockLength(uint32_t seconds)
  {
  }
  
  void Activity::setPrescales(uint32_t l1p, uint32_t hltp, uint32_t lb)
  {
    setL1Prescales(l1p);
    setHLTPrescales(hltp, lb);
  }
  
  void Activity::setBunchGroup(uint32_t)
  {
    std::string issue_txt = "HLTSV cannot set Bunch Group";
    hltsv::MasterTriggerIssue warn(ERS_HERE, issue_txt.c_str());
    ers::warning(warn);

  }
  
  void Activity::setConditionsUpdate(uint32_t, uint32_t)
  {
    std::string issue_txt = "HLTSV cannot set Conditions Update";
    hltsv::MasterTriggerIssue warn(ERS_HERE, issue_txt.c_str());
    ers::warning(warn);

  }

  
}
