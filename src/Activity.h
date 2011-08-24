// this -*- c++ -*-
#ifndef HLTSV_ACTIVITY_H_
#define HLTSV_ACTIVITY_H_

#include "ac/UserActivity.h"
#include "msginput/MessageDispatcher.h"
#include "msgconf/MessageConfiguration.h"
#include "queues/ProtectedQueue.h"

#include "RunController/CommandedTrigger.h"
#include "RunController/MasterTrigger.h"

#include "Scheduler.h"

#include "boost/thread/thread.hpp"

#include "tbb/concurrent_hash_map.h"

#include "TH1F.h"

#include "HLTSV.h"

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

    class Activity : public UserActivity, public daq::rc::MasterTrigger {
    public:
        Activity();
        ~Activity();
        
        DC::StatusWord act_config();
        DC::StatusWord act_connect();
        DC::StatusWord act_prepareForRun();
        DC::StatusWord act_disable(); 
        DC::StatusWord act_enable();
        DC::StatusWord act_stopL2SV();
        DC::StatusWord act_unconfig();
        DC::StatusWord act_userCommand();
        DC::StatusWord act_disconnect();
        DC::StatusWord act_exit();

        uint32_t hold();
        void resume();
        void setL1Prescales(uint32_t l1p);
        void setHLTPrescales(uint32_t hltp, uint32_t lb);
        void setPrescales(uint32_t  l1p, uint32_t hltp, uint32_t lb);
        void setLumiBlock(uint32_t lb, uint32_t runno);
        void setBunchGroup(uint32_t bg);
        void setConditionsUpdate(uint32_t folderIndex, uint32_t lb);

    private:
        void handle_announce(MessagePassing::Buffer *buffer);

        // these run as separate threads
        void handle_lvl1_input();
        void assign_event();
        void handle_decision();
        void handle_timeouts();
        void handle_clears();
        void update_rates();

    private:

        // typedef std::map<uint32_t,Event*> EventMap;
        typedef tbb::concurrent_hash_map<uint32_t,Event*> EventMap;

        MessageInput::MessageDispatcher m_dispatcher;
        MessageInput::InputThread       *m_input_thread;
        ProtectedQueue<Event*>          m_incoming_events;
        ProtectedQueue<uint32_t>        m_timeout_queue;
        ProtectedQueue<MessagePassing::Buffer*> m_decision_queue;
        ProtectedQueue<Event*>          m_clear_queue;
        Scheduler                       m_scheduler;
        EventMap                        m_events;

        daq::dynlibs::DynamicLibrary    *m_l1source_lib;
        hltsv::L1Source                 *m_l1source;

        hltsv::HLTSV                    m_stats;
        tdaq::sysmon::ISResource        *m_resource;
        TH1F                            *m_time;

        MessageConfiguration            m_msgconf;
        MessagePassing::Port            *m_ros_group;

        bool                            m_running;
        bool                            m_triggering;

        boost::thread                   *m_thread_input;
        std::vector<boost::thread*>     m_thread_assign;
        std::vector<boost::thread*>     m_thread_decision;
        boost::thread                   *m_thread_timeout;
        boost::thread                   *m_thread_clears;
        boost::thread                   *m_thread_update_rates;

        // master triger
        int                             m_triggerHoldCounter;
        daq::rc::CommandedTrigger       *m_cmdReceiver;
    };

}

#endif // HLTSV_ACTIVITY_H_

