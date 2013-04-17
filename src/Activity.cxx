
#include "Activity.h"

#include <stdlib.h>

#include "dynlibs/DynamicLibrary.h"
#include "clocks/Clock.h"

#include "config/Configuration.h"

#include "dal/Partition.h"

#include "DFdal/DFParameters.h"
#include "DFdal/DataFile.h"

#include "L1Source.h"
#include "LVL1Result.h"
#include "ROSClear.h"
#include "EventScheduler.h"

#include "hltsvdal/HLTSVApplication.h"
#include "hltsvdal/HLTSVConfiguration.h"

#include "RunController/ConfigurationBridge.h"

#include "monsvc/MonitoringService.h"
#include "monsvc/FilePublisher.h"

#include "asyncmsg/NameService.h"

#include "TFile.h"
#include <is/info.h>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <boost/thread.hpp>


#include <algorithm>

namespace hltsv {

  const size_t Num_Assign = 12;

  Activity::Activity(const std::string& name)
    : daq::rc::Controllable(name), 
      m_l1source_lib(nullptr),
      m_l1source(nullptr),
      m_time(),
      m_network(false),
      m_running(false),
      m_triggering(false)
  {
    m_work = new boost::asio::io_service::work(m_hltsv_io_service);
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
    IPCPartition               part(partition->UID());

    const daq::df::DFParameters *dfparams = conf->cast<daq::df::DFParameters>(partition->get_DataFlowParameters());

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

    // Initialize counters
    m_stats.LVL1Events = m_stats.AssignedEvents = m_stats.ProcessedEvents = m_stats.Timeouts = 
      m_stats.ProcessingNodesInitial = m_stats.ProcessingNodesDisabled = m_stats.ProcessingNodesEnabled =
      m_stats.ProcessingNodesAdded = 0;
    
    if(source_type == "internal" || source_type == "preloaded") {

      // TODO
      // m_cmdReceiver = new daq::rc::CommandedTrigger(p,getName(), this);
    }
        
    m_publisher.reset(new monsvc::PublishingController(part,getName()));
    
    m_publisher->add_configuration_rule(*monsvc::ConfigurationRule::from("DFObjects:.*/=>is:(2,DF)"));
    m_publisher->add_configuration_rule(*monsvc::ConfigurationRule::from("Histogramming:.*/=>oh:(5,DF,provider_name)"));
    
    m_time = monsvc::MonitoringService::instance().register_object("myTime",new TH1F("myTime","myTime", 2000, 0., 20000.));
       
    

    // Initialize  HLTSV_NameService
    // Declare it in .h?
    std::vector<std::string> data_networks = dfparams->get_DefaultDataNetworks();
    ERS_LOG("number of Data Networks  found: " << data_networks.size());
    data_networks.push_back("137.138.0.0/255.255.0.0"); 
    daq::asyncmsg::NameService HLTSV_NameService(part, data_networks);

    // Initialize ROS clear implementation
    m_ros_clear = std::make_shared<UnicastROSClear>(100, m_io_service, HLTSV_NameService);
    
    m_timeout = my_conf->get_Timeout();
    
    m_network = true;

    // Start ASIO server
    ERS_LOG(" *** Start io_service ***");
    auto func = [&] () {
      ERS_LOG(" *** Run io_service ***");
      m_hltsv_io_service.run(); 
      ERS_LOG(" *** io_service End ***");
    };
    boost::thread service_thread(func); // better movable thread (C++11)

    ERS_LOG(" *** Start HLTSVServer ***");
    m_event_sched = std::make_shared<EventScheduler>();
    m_myServer = std::make_shared<HLTSVServer> (m_hltsv_io_service, m_event_sched, m_ros_clear);
    // the id should be read from OKS
    std::string app_name = "HLTSV";
    m_myServer->listen(app_name);

    boost::asio::ip::tcp::endpoint my_endpoint = m_myServer->localEndpoint();
    ERS_LOG("Port Used: " << my_endpoint.port() );
    ERS_LOG("IP Address: " <<my_endpoint.address() );

    // Publish port in IS for DCM
    HLTSV_NameService.publish(app_name, my_endpoint.port());

    //  HLTSVServer::start() calls Server::asyncAccept() which calls HLTSVServer::onAccept
    m_myServer->start();
    return;
  }

  void Activity::connect(std::string& )
  {
      m_publisher->start_publishing();
      return;
  }

  void Activity::prepareForRun(std::string& )
  {
    m_stats.LVL1Events = m_stats.AssignedEvents = m_stats.ProcessedEvents = m_stats.Timeouts = 
      m_stats.ProcessingNodesDisabled = m_stats.ProcessingNodesEnabled =
      m_stats.ProcessingNodesAdded = 0;
    
    m_l1source->reset();

    m_running = true;
    m_triggering = true;

    // m_thread_update_rates = new boost::thread(&Activity::update_rates, this);
    
    auto func = [&] () {
      ERS_LOG(" *** triggering L1Source ***");
      while(m_triggering) {
	std::shared_ptr<LVL1Result> result(m_l1source->getResult());
	m_event_sched->schedule_event(result);
	ERS_LOG("L1ID #" << result->l1_id() << "  has been scheduled");
      }
      ERS_LOG(" *** trigger_thread End ***");
    };
    boost::thread trigger_thread(func); // better movable thread (C++11)

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
  
  //DC::StatusWord Activity::act_stopL2SV()
  void Activity::stopL2SV(std::string &)
  {
    m_triggering = false;
    m_running = false;
    
    return;
  }

  //DC::StatusWord Activity::act_unconfig()
  void Activity::unconfigure(std::string &)
  {
    if(m_cmdReceiver) {
      m_cmdReceiver->_destroy();
      m_cmdReceiver = 0;
    }
    
    m_network = false;
    
    delete m_l1source;
    m_l1source = 0;
    m_l1source_lib->release();
    delete m_l1source_lib;
    
    const std::unique_ptr<TFile> outfile(new TFile("test.root","recreate"));
    
    boost::shared_ptr<monsvc::FilePublisher> pub(boost::make_shared<monsvc::FilePublisher>(outfile.get()));
    monsvc::NameFilter filter;
    monsvc::PublishingController::publish_now(pub,filter);

    // done at delete ??
    //
    // outfile->Write();
    // outfile->Close();

    // ? who deletes m_time ?

    //delete m_time;
    //m_time = 0;
    
    return;
  }

  void Activity::disconnect(std::string & )
  {
      m_publisher->stop_publishing();
      return;// DC::OK;
  }

#if 0
  void Activity::update_rates()
  {
    while(m_running) {
      uint64_t oldProcessedEvents = m_stats.ProcessedEvents;
      sleep(5);
      m_stats.Rate = (m_stats.ProcessedEvents - oldProcessedEvents)/5.0;
    }
  }
#endif
  
  /**
   *     MasterTrigger interface
   **/

    MasterTrigger::MasterTrigger(L1Source *l1source, bool& triggering)
        : m_l1source(l1source),
          m_triggering(triggering),
          m_triggerHoldCounter(0)
    {
    }
    
  uint32_t MasterTrigger::hold()
  {
    m_triggerHoldCounter += 1;
    m_triggering = false;
    return 0;
  }
  
  void MasterTrigger::resume()
  {
    if (m_triggerHoldCounter > 0) {
      m_triggerHoldCounter -= 1;
    }
    
    if (m_triggerHoldCounter == 0) {
      m_triggering = true;
    } 
  }
  
  void MasterTrigger::setL1Prescales(uint32_t )
  {
  }
  
  void MasterTrigger::setHLTPrescales(uint32_t , uint32_t lb)
  {
    m_l1source->setHLTCounter(lb);
  }
  
  void MasterTrigger::setLumiBlock(uint32_t lb, uint32_t )
  {
    m_l1source->setLB(lb);
  }
  
  void MasterTrigger::setPrescales(uint32_t  l1p, uint32_t hltp, uint32_t lb)
  {
    setL1Prescales(l1p);
    setHLTPrescales(hltp, lb);
  }
  
  void MasterTrigger::setBunchGroup(uint32_t)
  {
  }
  
  void MasterTrigger::setConditionsUpdate(uint32_t, uint32_t)
  {
  }
  
}
