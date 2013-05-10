// this -*- c++ -*-
#ifndef HLTSV_ACTIVITY_H_
#define HLTSV_ACTIVITY_H_

// Run control related
#include "RunController/Controllable.h"
#include "RunController/CommandedTrigger.h"
#include "RunController/MasterTrigger.h"

// Monitoring
#include "TH1F.h"

#include "monsvc/PublishingController.h"

// for the io_service
#include "HLTSVServer.h"

#include <vector>
#include <thread>
#include <memory>

// for dynamic loading of L1Source
namespace daq { 
    namespace dynlibs {
        class DynamicLibrary;
    }
}

namespace hltsv {

  class L1Source;
  class EventScheduler;
  class ROSClear;

  /**
   * \brief Main application functionality of HLTSV.
   *
   * This class implements the run control and master trigger interface
   * and all associated actions.
   */

  class Activity : public daq::rc::Controllable, public daq::rc::MasterTrigger {
  public:
    Activity(const std::string&);
    ~Activity();
    
    // Run control commands
    void configure(std::string&) override;
    void connect(std::string&) override;
    void prepareForRun(std::string&) override;
    void disable(std::string&) override;
    void enable(std::string&) override;
    void stopL2SV(std::string&) override;
    void stopSFO(std::string&) override;
    void unconfigure(std::string&) override;
    void disconnect(std::string&) override;

    // Master Trigger commands
    uint32_t hold();
    void resume();
    void setL1Prescales(uint32_t l1p);
    void setHLTPrescales(uint32_t hltp, uint32_t lb);
    void setPrescales(uint32_t  l1p, uint32_t hltp, uint32_t lb);
    void setLumiBlock(uint32_t lb, uint32_t runno);
    void setBunchGroup(uint32_t bg);
    void setConditionsUpdate(uint32_t folderIndex, uint32_t lb);

  private:

    // The internal boost::asio::io_service::run() thread.
    void io_thread(boost::asio::io_service& service);

    // The internal thread for reading the RoIB
    void l1_thread();

    // To delay events in SV
    unsigned int                   m_event_delay;
    
    // To keep the io_service running.
    std::unique_ptr<boost::asio::io_service::work> m_work;
    std::unique_ptr<boost::asio::io_service::work> m_ros_work;

    // io_service for DCM communication.
    boost::asio::io_service        m_io_service;

    // io_service for ROS communication
    boost::asio::io_service        m_ros_io_service;

    // HLTSVServer accepting incoming connections from the DCM
    std::shared_ptr<HLTSVServer>   m_myServer;

    // Thread to read RoIB
    std::thread                    m_l1_thread;

    // Thread pool to handle DCM communication.
    std::vector<std::thread>       m_io_threads;
    
    // Event Scheduler
    std::shared_ptr<EventScheduler> m_event_sched;
    
    // L1 Source
    daq::dynlibs::DynamicLibrary    *m_l1source_lib;
    hltsv::L1Source                 *m_l1source;
    
    // ROS Clear interface
    std::shared_ptr<ROSClear>       m_ros_clear;
    
    // Monitoring
    std::unique_ptr<monsvc::PublishingController> m_publisher;
    
    // Running flags

    // The network is active.
    bool                            m_network;

    // We are in running state.
    bool                            m_running;

    // We are in triggering state.
    // This can be different from running if we are the master trigger.
    bool                            m_triggering;

    // for MasterTrigger interface
    std::unique_ptr<daq::rc::CommandedTrigger> m_cmdReceiver;

    // hold trigger counter
    int                             m_triggerHoldCounter;
    // hold trigger flag
    bool                            m_masterTrigger;
    
  };
}

#endif // HLTSV_ACTIVITY_H_

