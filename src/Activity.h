// this -*- c++ -*-
#ifndef HLTSV_ACTIVITY_H_
#define HLTSV_ACTIVITY_H_

// Run control related
#include "RunControl/Common/Controllable.h"

// MasterTrigger
#include "TriggerCommander/CommandedTrigger.h"
#include "TriggerCommander/MasterTrigger.h"

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

  class Activity : public daq::rc::Controllable, public daq::trigger::MasterTrigger {
  public:
    Activity();
    ~Activity() noexcept;
    
    // Run control commands
    void configure(const daq::rc::TransitionCmd& ) override;
    void connect(const daq::rc::TransitionCmd& ) override;
    void prepareForRun(const daq::rc::TransitionCmd& ) override;
    void stopDC(const daq::rc::TransitionCmd& ) override;
    void stopRecording(const daq::rc::TransitionCmd& ) override;
    void unconfigure(const daq::rc::TransitionCmd& ) override;
    void disconnect(const daq::rc::TransitionCmd& ) override;

    // Master Trigger commands
    uint32_t hold();
    void resume();
    void setL1Prescales(uint32_t l1p) override;
    void setHLTPrescales(uint32_t hltp, uint32_t lb) override;
    void setPrescales(uint32_t  l1p, uint32_t hltp, uint32_t lb) override;
    void setLumiBlock(uint32_t lb, uint32_t runno) override;
    void setBunchGroup(uint32_t bg) override;
    void setConditionsUpdate(uint32_t folderIndex, uint32_t lb) override;
    void increaseLumiBlock(uint32_t) override;

  private:

    // The internal boost::asio::io_service::run() thread.
    void io_thread(boost::asio::io_service& service);

    // The internal thread for reading the RoIB
    void l1_thread();

    // To delay events in SV
    unsigned int                   m_event_delay;
    
    // To keep the io_service running.
    std::unique_ptr<std::vector<boost::asio::io_service::work>> m_work;
    std::unique_ptr<boost::asio::io_service::work> m_ros_work;

    // io_service for DCM communication.
    std::unique_ptr<std::vector<boost::asio::io_service>> m_io_services;

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
    std::vector<std::unique_ptr<daq::dynlibs::DynamicLibrary>>  m_l1source_libs;
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
    std::unique_ptr<daq::trigger::CommandedTrigger> m_cmdReceiver;

    // hold trigger counter
    int                             m_triggerHoldCounter;
    // hold trigger flag
    bool                            m_masterTrigger;
    
  };
}

#endif // HLTSV_ACTIVITY_H_

