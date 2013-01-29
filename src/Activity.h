// this -*- c++ -*-
#ifndef HLTSV_ACTIVITY_H_
#define HLTSV_ACTIVITY_H_

//#include "ac/UserActivity.h"
#include "RunController/Controllable.h"

#include "msgconf/MessageConfiguration.h"
#include "queues/ProtectedQueue.h"

#include "RunController/CommandedTrigger.h"
#include "RunController/MasterTrigger.h"

#include "Scheduler.h"

#include "boost/thread/thread.hpp"
#include "boost/thread/mutex.hpp"

#include "tbb/concurrent_hash_map.h"

#include "TH1F.h"

#include "HLTSV.h"

#include "ipc/core.h"
#include "ipc/object.h"
#include "ipc/partition.h"

#include "monsvc/ptr.h"
#include "monsvc/PublishingController.h"
#include "monsvc/NameFilter.h"
#include "monsvc/FilePublisher.h"
#include "monsvc/OstreamPublisher.h"
#include "monsvc/ISPublisher.h"
#include "monsvc/OHPublisher.h"

namespace dcmessages {
    class LVL1Result;
}

namespace MessagePassing {
    class Buffer;
}

namespace MessageInput {
    class InputThread;
}

namespace daq { 
    namespace dynlibs {
        class DynamicLibrary;
    }
}

namespace tdaq {
    namespace sysmon {
        class ISResource;
    }
}

namespace hltsv {
  class Event;
  class L1Source;
  class Activity : public daq::rc::Controllable, 
		   public daq::rc::MasterTrigger 
  {

    using MasterTrigger::resume;
  public:
    Activity(std::string&);
    ~Activity();
    
    void configure(std::string&);
    void connect(std::string&);
    void prepareForRun(std::string&);
    void disable(std::string&);
    void enable(std::string&);
    void stopL2SV(std::string&);
    void unconfigure(std::string&);
    //void userCommand(std::string&, std::string&);
    void disconnect(std::string&);

    
    uint32_t hold();
    void resume();
    void setL1Prescales(uint32_t l1p);
    void setHLTPrescales(uint32_t hltp, uint32_t lb);
    void setPrescales(uint32_t  l1p, uint32_t hltp, uint32_t lb);
    void setLumiBlock(uint32_t lb, uint32_t runno);
    void setBunchGroup(uint32_t bg);
    void setConditionsUpdate(uint32_t folderIndex, uint32_t lb);
    
  private:
    // these run as separate threads
    void handle_lvl1_input();
    void assign_event();
    void handle_network();
    void update_rates();
    
    // helper functions
    void handle_announce(MessagePassing::Buffer *buffer);
    void handle_decision(MessagePassing::Buffer *buffer);
    void handle_timeouts();
    void add_event_to_clear(uint32_t lvl1_id);
    
  private:
    
    // typedef std::map<uint32_t,Event*> EventMap;
    typedef tbb::concurrent_hash_map<uint32_t,Event*> EventMap;
    
    ProtectedQueue<Event*>          m_incoming_events;
    
    Scheduler                       m_scheduler;
    EventMap                        m_events;
    
    daq::dynlibs::DynamicLibrary    *m_l1source_lib;
    hltsv::L1Source                 *m_l1source;
    
    uint32_t                        m_timeout;
    
    hltsv::HLTSV                    m_stats;
    tdaq::sysmon::ISResource        *m_resource;
    TH1F                            *m_time;
    
    MessageConfiguration            m_msgconf;
    MessagePassing::Port            *m_ros_group;
    
    bool                            m_network;
    bool                            m_running;
    bool                            m_triggering;
    
    unsigned int                    m_num_assign_threads;
    boost::thread                   *m_thread_input;
    std::vector<boost::thread*>     m_thread_assign;
    boost::thread                   *m_thread_decision;
    boost::thread                   *m_thread_update_rates;
    
    std::vector<uint32_t>           m_to_clear;
    
    // master triger
    int                             m_triggerHoldCounter;
    daq::rc::CommandedTrigger       *m_cmdReceiver;
    monsvc::PublishingController    *pc;
    TFile                           *outfile;
  };
  
}

#endif // HLTSV_ACTIVITY_H_

