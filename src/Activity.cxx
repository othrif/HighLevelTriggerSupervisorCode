
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
#include "DFdal/RoIBMasterTriggerPlugin.h"

#include "RunControl/Common/OnlineServices.h"
#include "RunControl/Common/RunControlCommands.h"
#include "RunControl/FSM/FSMStates.h"

#include "monsvc/MonitoringService.h"

#include "HLTSV.h"

#include "Issues.h"
#include "ers/ers.h"

#include "asyncmsg/NameService.h"

#include <is/info.h>

#include <algorithm>
#include <chrono>
#include <ctime>
#include <boost/algorithm/string.hpp>

namespace {
  std::uint64_t ns()
  {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return std::uint64_t(ts.tv_sec) * 1000000000 + std::uint64_t(ts.tv_nsec);
  }
}

namespace hltsv {

  Activity::Activity(bool restarted)
    : daq::rc::Controllable(), 
      m_l1source(nullptr),
      m_network(false),
      m_running(false),
      m_cmdReceiver(0),
      m_restarted(restarted)
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
    file_names.resize(dataFiles.size());
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

    if(source->cast<daq::df::RoIBMasterTriggerPlugin>()) {
        m_cmdReceiver = new daq::trigger::CommandedTrigger(part, daq::rc::OnlineServices::instance().applicationName(), m_l1source);
    }

    m_publisher.reset(new monsvc::PublishingController(part,"HLTSV"));
    m_publisher->add_configuration_rules(conf, self);

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
    m_stats = monsvc::MonitoringService::instance().register_object("Events",new HLTSV(), true);
    m_event_sched = std::make_shared<EventScheduler>(m_stats);
    m_myServer = std::make_shared<HLTSVServer> (*m_io_services, m_event_sched, m_ros_clear, self->get_Timeout(), m_stats);

    m_myServer->listen(daq::rc::OnlineServices::instance().applicationName());

    boost::asio::ip::tcp::endpoint my_endpoint = m_myServer->localEndpoint();
    ERS_LOG("Local endpoint: " << my_endpoint);

    // Publish port in IS for DCM
    HLTSV_NameService.publish(daq::rc::OnlineServices::instance().applicationName(), my_endpoint.port());

    // restart handling, try to read old IS information and continue from there.
    if(m_restarted) {
        // This throws an exception, e.g. because the HLTSV information is not there
        // we want to stop, since we have no idea with which event ID to recover. So let
        // the IS exception just propagate.
        HLTSV hltsv_info;
        ISInfoDictionary dict(daq::rc::OnlineServices::instance().getIPCPartition());
        dict.getValue("DF.HLTSV.Events",hltsv_info);

        m_initial_event_id = hltsv_info.Recent_Global_ID + 10000000;

        ERS_LOG("HLTSV has been restarted during run; starting with global event ID" << m_initial_event_id);
    } else {
        m_initial_event_id = 0;
    }

    // clear statistics, counters, event queues
    m_event_sched->reset(m_initial_event_id);
    m_stats->reset();

    m_initial_event_id = 0;

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
    m_ros_clear->prepareForRun();

    const IPCPartition  partition(daq::rc::OnlineServices::instance().getIPCPartition());

    RunParamsNamed runparams(partition, "RunParams.SOR_RunParams");
    runparams.checkout();

    m_l1source->reset(runparams.run_number);
    m_max_events = runparams.max_events;

    // For the FILAR input we need a second reset after
    // a delay to make it *really* work.
    //
    // This is safe for the other plugins as well, since they
    // just reset some counters and clear data structures.
    usleep(500000);
    m_l1source->reset(runparams.run_number);    

    if(m_cmdReceiver) {
        m_l1source->startLumiBlockUpdate();
    }

    m_running = true;

    m_l1_thread = std::thread(&Activity::l1_thread, this);
    
    return;
  }
    
  void Activity::stopDC(const daq::rc::TransitionCmd& )
  {

    m_event_sched->push_events();

    m_running = false;
    if(m_cmdReceiver) {
        m_l1source->stopLumiBlockUpdate();
    }

    m_l1_thread.join();
    
    return;
  }

  void Activity::stopRecording(const daq::rc::TransitionCmd& )
  {
    // reset scheduler
    m_event_sched->reset(0);
    m_ros_clear->flush();
  }


  void Activity::unconfigure(const daq::rc::TransitionCmd& )
  {

    ERS_LOG("HLTSV is UNCONFIGURING");

    m_myServer->stop();

    m_network = false;

    m_work.reset();
    m_ros_work.reset();
    m_event_sched.reset();
    m_ros_clear.reset();
    m_myServer.reset();

    monsvc::MonitoringService::instance().remove_object(std::string("Events"));

    for(auto& thr : m_io_threads) {
        thr.join();
    }
    m_io_threads.clear();

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

    if(m_cmdReceiver) {
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

  void Activity::user(const daq::rc::UserCmd& cmd)
  {
    if (cmd.commandName() == "HLTSV_SET_DELAY") {
      if (daq::rc::FSMStates::stringToState(cmd.currentFSMState()) < daq::rc::FSM_STATE::CONFIGURED) {
        ERS_LOG("Received HLTSV_SET_DELAY command, but the application is not yet configured. Ignoring.");
      } else if (cmd.commandParameters().size() != 1) {
        ERS_LOG("Received HLTSV_SET_DELAY command with "
                << cmd.commandParameters().size() <<
                " parameters, but one parameter is needed. Ignoring.");
      } else {
        // Parse parameter
        auto param = cmd.commandParameters().front();
        if (param == "") {
          // Read value from config
          auto& conf = daq::rc::OnlineServices::instance().getConfiguration();
          auto* self = conf.cast<daq::df::HLTSVApplication>(&daq::rc::OnlineServices::instance().getApplication());
          m_event_delay = self->get_EventDelay();
        } else {
          // Parse value
          try {
            m_event_delay = boost::lexical_cast<unsigned int>(param);
            ERS_LOG("Received HLTSV_SET_DELAY command with parameter "
                    << cmd.commandParameters().front() <<
                    ": target event delay updated to << "
                    << m_event_delay << ".");
          } catch (boost::bad_lexical_cast& ex) {
            ERS_LOG("Received HLTSV_SET_DELAY command with parameter "
                    << cmd.commandParameters().front() <<
                    ", but could not parse parameter: " << ex.what()
                    << ". Ignoring.");
          }
        }
      }
    } else {
      ERS_LOG("Received unknown user command: " << cmd.commandName()
              << ". Ignoring.");
    }
  }

  void Activity::l1_thread()
  {
    ERS_LOG("Starting l1 thread");

    const std::uint64_t ms = 1000ull;
    const std::uint64_t s = 1000ull * 1000ull;
    std::uint64_t deadline = ns() + m_event_delay * ms;

    uint64_t m_assigned = 0;   // counter of events from L1Source

    while(m_running) {

        try {
            std::shared_ptr<LVL1Result> result(m_l1source->getResult());
            if(result) {
                m_event_sched->schedule_event(result);
                ERS_DEBUG(1,"L1ID #" << result->l1_id() << "  has been scheduled");
                m_assigned++;
                
                if(m_cmdReceiver && m_max_events > 0 && m_assigned == m_max_events) {
                    m_running = false;
                }
            }
        } catch (ers::Issue& ex) {
            ers::error(ex);
        } catch (std::exception& ex) {
            ERS_LOG("Unexcpected std::exception " << ex.what());
        } catch (...) {
            ERS_LOG("Unknown exception thrown");
        }
        
        std::uint64_t now = ns();
        std::uint64_t period = m_event_delay * ms;
        if (period != 0) {
          if (now > deadline + 1 * s) {
            // We overshot the deadline by more than 1 s. 
            // Reset the next deadline
            // and go immediately back to scheduling
            deadline = now + period;
          } else {
            // Busy-wait until the deadline is reached.
            while (now < deadline) {
              now = ns();
            }
            deadline += period;
          }
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

  void Activity::update_monitoring(HLTSV *info) 
  {
      if(m_l1source) {
          m_l1source->getMonitoringInfo(info);
      }
  }

}
