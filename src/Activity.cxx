
#include "Activity.h"

#include <stdlib.h>

#include "dynlibs/DynamicLibrary.h"

#include "config/Configuration.h"

#include "dal/Partition.h"
#include "dal/MasterTrigger.h"

#include "DFdal/DFParameters.h"
#include "DFdal/DataFile.h"

#include "L1Source.h"
#include "LVL1Result.h"
#include "ROSClear.h"
#include "UnicastROSClear.h"
#include "MulticastROSClear.h"
#include "EventScheduler.h"

#include "DFdal/HLTSVApplication.h"
#include "DFdal/HLTSVConfiguration.h"

#include "RunController/ConfigurationBridge.h"

#include "monsvc/MonitoringService.h"
#include "monsvc/FilePublisher.h"
#include "HLTSV.h"

#include "asyncmsg/NameService.h"

#include <is/info.h>

#include <algorithm>

namespace hltsv {

  const size_t Num_Assign = 12;

  Activity::Activity(const std::string& name)
    : daq::rc::Controllable(name), 
      m_l1source_lib(nullptr),
      m_l1source(nullptr),
      m_network(false),
      m_running(false),
      m_triggering(false),
      m_triggerHoldCounter(0),
      m_masterTrigger(false)
  {
  }

  Activity::~Activity()
  {
  }
  
  void Activity::configure(std::string &)
  {

    ERS_LOG(" *** ENTER IN Activity::configure() ***");

    Configuration* conf = daq::rc::ConfigurationBridge::instance()->getConfiguration();

    const daq::df::HLTSVApplication *self = conf->cast<daq::df::HLTSVApplication>(daq::rc::ConfigurationBridge::instance()->getApplication());
    const daq::df::HLTSVConfiguration *my_conf = self->get_Configuration();

    const daq::core::Partition *partition = daq::rc::ConfigurationBridge::instance()->getPartition();
    const IPCPartition          part(partition->UID());

    const daq::df::DFParameters *dfparams = conf->cast<daq::df::DFParameters>(partition->get_DataFlowParameters());

    m_event_delay = my_conf->get_EventDelay();

    // Load L1 Source
    std::vector<std::string> file_names;

    const std::vector<const daq::df::DataFile*>& dataFiles = dfparams->get_UsesDataFiles();
    std::transform(dataFiles.begin(), dataFiles.end(), file_names.begin(), [](const daq::df::DataFile* df) { return df->get_FileName(); });

    std::string source_type = my_conf->get_L1Source();
    std::string lib_name("libsvl1");
    lib_name += source_type;

    try {
      m_l1source_lib = new DynamicLibrary(lib_name);
      if(L1Source::creator_t make = m_l1source_lib->function<L1Source::creator_t>("create_source")) {
          // fix me: get data files
        m_l1source = make(source_type, std::vector<std::string>());
	m_l1source->preset();
      } else {
	// fatal
      }
    } catch (ers::Issue& ex) {
      ers::fatal(ex);
      return;
    }


    // Conditions for which the HLTSV should be Master Trigger
    const daq::core::MasterTrigger* masterholder = partition->get_MasterTrigger();
    if (masterholder) {
      if(source_type == "internal" || source_type == "preloaded") {
	// Check if OKS is set correctly
	if(masterholder->UID() == self->UID()) {
	  // HLTSV is master trigger
	  m_masterTrigger = true;
	  m_cmdReceiver.reset(new daq::rc::CommandedTrigger(part, getName(), this));
	} else {
	  // fatal
	  ERS_LOG("HLTSV is not the master trigger, the master trigger is: " << masterholder->UID());
	}   
      } else {
	if(masterholder->UID() == self->UID()) {
	  // warning?
	  ERS_LOG("HLTSV is set as the master trigger, but the source type is: " << source_type);
	}
      }
    } else {
      // warning?
      ERS_LOG("Master Trigger not defined in the partition");
    }

    m_publisher.reset(new monsvc::PublishingController(part,getName()));
    m_publisher->add_configuration_rules(*conf);

    m_io_services.reset(new std::vector<boost::asio::io_service>(my_conf->get_NumberOfAssignThreads()));

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
        m_ros_clear = std::make_shared<UnicastROSClear>(my_conf->get_ClearGrouping(), m_ros_io_service, HLTSV_NameService);
    } else {

        // address is format  <Multicast-IP-Adress>/<OutgoingInterface>

        auto mc = dfparams->get_MulticastAddress();
        auto n = mc.find('/');
        auto mcast = mc.substr(0, n);
        auto outgoing = mc.substr(n + 1);

        ERS_LOG("Configuring for multicast: " << mcast << '/' << outgoing);

        m_ros_clear = std::make_shared<MulticastROSClear>(my_conf->get_ClearGrouping(), m_ros_io_service, mcast, outgoing);
    }
    
    m_network = true;

    ERS_LOG(" *** Start HLTSVServer ***");
    m_event_sched = std::make_shared<EventScheduler>();
    m_myServer = std::make_shared<HLTSVServer> (*m_io_services, m_event_sched, m_ros_clear, my_conf->get_Timeout());
    // the id should be read from OKS
    m_myServer->listen(getName());

    boost::asio::ip::tcp::endpoint my_endpoint = m_myServer->localEndpoint();
    ERS_LOG("Local endpoint: " << my_endpoint);

    // Publish port in IS for DCM
    HLTSV_NameService.publish(getName(), my_endpoint.port());

    //  HLTSVServer::start() calls Server::asyncAccept() which calls HLTSVServer::onAccept
    m_myServer->start();

    for(unsigned int i = 0; i < my_conf->get_NumberOfAssignThreads(); i++) {
        m_io_threads.push_back(std::thread(&Activity::io_thread, this, std::ref((*m_io_services)[i])));
    }

    m_io_threads.push_back(std::thread(&Activity::io_thread, this, std::ref(m_ros_io_service)));

    return;
  }

  void Activity::connect(std::string& )
  {
    // ROS_Clear with TCP will establish the connection
    m_ros_clear->connect();
    
    m_publisher->start_publishing();
    return;
  }

  void Activity::prepareForRun(std::string& )
  {
    m_l1source->reset();

    m_running = true;
    m_triggering = true;

    m_l1_thread = std::thread(&Activity::l1_thread, this);
    
    return; 
  }
    
  void Activity::disable(std::string & )
  {
    return;
  }
    
  void Activity::enable(std::string &)
  {
    return;
  }
  

  void Activity::stopL2SV(std::string &)
  {

    m_event_sched->push_events();

    m_triggering = false;
    m_running = false;


    m_l1_thread.join();
    
    return;
  }

  void Activity::stopSFO(std::string& )
  {
    m_ros_clear->flush();
  }


  void Activity::unconfigure(std::string &)
  {

    m_network = false;

    m_myServer->stop();

    m_work.reset();
    m_ros_work.reset();

    m_event_sched.reset();
    m_ros_clear.reset();
    m_myServer.reset();

    // m_io_service.stop();
    // m_ros_io_service.stop();

    for(auto& thr : m_io_threads) {
        thr.join();
    }

    delete m_l1source;
    m_l1source = 0;
    m_l1source_lib->release();
    delete m_l1source_lib;

    for(auto& svc : *m_io_services) {
        svc.reset();
    }
    m_ros_io_service.reset();

    return;
  }

  void Activity::disconnect(std::string & )
  {
      m_publisher->stop_publishing();
      return;// DC::OK;
  }

  void Activity::l1_thread()
  {
      ERS_LOG("Starting l1 thread");
      while(m_running) {
          if(m_triggering) {

              if(m_event_delay > 0) {
                  usleep(m_event_delay);
              }

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
  }
  
  void Activity::setHLTPrescales(uint32_t , uint32_t lb)
  {
    m_l1source->setHLTCounter(lb);
  }
  
  void Activity::setLumiBlock(uint32_t lb, uint32_t )
  {
    m_l1source->setLB(lb);
  }
  
  void Activity::setPrescales(uint32_t l1p, uint32_t hltp, uint32_t lb)
  {
    setL1Prescales(l1p);
    setHLTPrescales(hltp, lb);
  }
  
  void Activity::setBunchGroup(uint32_t)
  {
  }
  
  void Activity::setConditionsUpdate(uint32_t, uint32_t)
  {
  }
  
}
