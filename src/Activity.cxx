
#include "Activity.h"

#include "dynlibs/DynamicLibrary.h"
#include "dcmessages/Messages.h"
#include "dcmessages/LVL1Result.h"
#include "dcmessages/DFM_Clear_Msg.h"
#include "msg/Buffer.h"
#include "msg/Port.h"
#include "msginput/InputThread.h"
#include "msginput/MessageHeader.h"

#include "ac/AppControl.h"
#include "config/Configuration.h"

#include "dal/Partition.h"

#include "sysmonapps/ISResource.h"
#include "hltinterface/ITHistRegister.h"

#include "Event.h"
#include "Node.h"
#include "L1Source.h"

#include "boost/function.hpp"

namespace hltsv {

    const size_t Num_Assign = 8;
    const size_t Num_Decision = 8;

    Activity::Activity()
        : m_input_thread(0),
          m_incoming_events(64000),
          m_l1source_lib(0),
          m_l1source(0),
          m_resource(0),
          m_time(0),
          m_ros_group(0),
          m_running(false),
          m_triggering(false),
          m_thread_input(0),
          m_thread_timeout(0),
          m_thread_clears(0),
          m_triggerHoldCounter(0)
    {
    }

    Activity::~Activity()
    {
    }
        
    DC::StatusWord Activity::act_config()
    {
        m_dispatcher.register_handler(0x12345678, boost::bind(&Activity::handle_announce, this, _1));
        m_dispatcher.register_queue(0x87654321, &m_decision_queue);

        m_input_thread = new MessageInput::InputThread(&m_dispatcher);

        Configuration* conf;
        AppControl::get_instance()->getConfiguration(&conf);

        std::string source_type = "internal";
        std::string lib_name("libsvl1");
        lib_name += source_type;

        try {
            m_l1source_lib = new DynamicLibrary(lib_name);
            if(L1Source::creator_t make = m_l1source_lib->function<L1Source::creator_t>("create_source")) {
                m_l1source = make(source_type, *conf);
                m_l1source->preset();
            } else {
                // fatal
            }
        } catch (ers::Issue& ex) {
            ers::fatal(ex);
            return DC::FATAL;
        }

        AppControl *ac = AppControl::get_instance();

        if(!m_msgconf.configure(ac->getNodeID(), *conf)) {
            // fatal
        }

        m_ros_group = m_msgconf.create_group("ROS");
        ERS_LOG("ROS port = " << m_ros_group);

        typedef std::list<MessagePassing::Port*> PortList;
        PortList efds = m_msgconf.create_by_group("L2PU");
        if(efds.size() == 0) {
            // no nodes
            ERS_LOG("No processing nodes available");
            return DC::FATAL;
        }

        for(PortList::iterator it = efds.begin(); it != efds.end(); ++it) {
            m_scheduler.add_node(new Node(*it, 8));
        }


        m_stats.LVL1Events = m_stats.AssignedEvents = m_stats.ProcessedEvents = m_stats.Timeouts = 
            m_stats.ProcessingNodesInitial = m_stats.ProcessingNodesDisabled = m_stats.ProcessingNodesEnabled =
            m_stats.ProcessingNodesAdded = 0;

        m_stats.ProcessingNodesInitial = efds.size();

        m_resource = new tdaq::sysmon::ISResource("HLTSV", m_stats);

        if(source_type == "internal" || source_type == "preloaded") {
            m_cmdReceiver = new daq::rc::CommandedTrigger(IPCPartition(ac->get_instance()->getPartition()->UID()),
                                                          ac->get_instance()->getAppName(), this);
        }

        m_time = new TH1F("Time","Time", 2000, 0., 20000.);

        hltinterface::ITHistRegister::instance()->registerTObject("", "/DEBUG/HLTSV/Time", m_time);

        return DC::OK;
    }

    DC::StatusWord Activity::act_connect()
    {
        MessagePassing::Port::connect(m_msgconf.create_by_group("L2PU"));
        m_input_thread->start();
        return DC::OK;
    }

    DC::StatusWord Activity::act_prepareForRun()
    {
        m_stats.LVL1Events = m_stats.AssignedEvents = m_stats.ProcessedEvents = m_stats.Timeouts = 
            m_stats.ProcessingNodesDisabled = m_stats.ProcessingNodesEnabled =
            m_stats.ProcessingNodesAdded = 0;

        m_l1source->reset();

        m_running = true;
        m_triggering = true;

        m_thread_input    = new boost::thread(&Activity::handle_lvl1_input, this);

        for(size_t i = 0; i < Num_Assign; i++) {
            m_thread_assign.push_back(new boost::thread(&Activity::assign_event, this));
        }

        for(size_t i = 0; i < Num_Decision; i++) {
            m_thread_decision.push_back(new boost::thread(&Activity::handle_decision, this));
        }

        m_thread_timeout  = new boost::thread(&Activity::handle_timeouts, this);
        m_thread_clears   = new boost::thread(&Activity::handle_clears, this);

        m_thread_update_rates = new boost::thread(&Activity::update_rates, this);


        return DC::OK;
    }

    DC::StatusWord Activity::act_disable()
    {
        return DC::OK;
    }

    DC::StatusWord Activity::act_enable()
    {
        return DC::OK;
    }

    DC::StatusWord Activity::act_stopL2SV()
    {
        m_triggering = false;
        m_running = false;

        usleep(1000);

        m_incoming_events.wakeup();
        m_decision_queue.wakeup();
        m_clear_queue.wakeup();

        m_thread_input->join();
        delete m_thread_input;

        for(size_t i = 0; i < Num_Assign; i++) {
            m_thread_assign[i]->join();
            delete m_thread_assign[i];
        }
        m_thread_assign.clear();

        for(size_t i = 0; i < Num_Decision; i++) {
            m_thread_decision[i]->join();
            delete m_thread_decision[i];
        }
        m_thread_decision.clear();

        m_thread_timeout->join();
        delete m_thread_timeout;

        m_thread_clears->join();
        delete m_thread_clears;

        m_thread_update_rates->join();
        delete m_thread_update_rates;

        return DC::OK;
    }

    DC::StatusWord Activity::act_unconfig()
    {
        if(m_cmdReceiver) {
            m_cmdReceiver->_destroy();
            m_cmdReceiver = 0;
        }

        delete m_l1source;
        m_l1source = 0;
        m_l1source_lib->release();
        delete m_l1source_lib;

        m_dispatcher.unregister_type_handler(0x87654321);
        m_dispatcher.unregister_type_handler(0x12345678);

        m_input_thread->stop();
        m_input_thread->wait();
        delete m_input_thread;
        
        m_msgconf.unconfigure();

        delete m_resource;
        m_resource = 0;

        hltinterface::ITHistRegister::instance()->releaseTObject("", "/DEBUG/HLTSV/Time");
        delete m_time;
        m_time = 0;

        return DC::OK;
    }

    DC::StatusWord Activity::act_userCommand()
    {
        return DC::OK;
    }

    DC::StatusWord Activity::act_disconnect()
    {
        return DC::OK;
    }

    DC::StatusWord Activity::act_exit()
    {
        return DC::OK;
    }

    void Activity::handle_announce(MessagePassing::Buffer *buffer)
    {
        using namespace MessagePassing;
        using namespace MessageInput
;
        MessageHeader header(buffer);
        if(header.valid()) {
            // should contain: NodeID, number of processing slots
            Buffer::iterator it = buffer->begin();
            it += MessageHeader::SIZE;
            NodeID id = header.source();
            unsigned int  slots = 8;
            it >> slots;

            if(Node *node = m_scheduler.find_node(id)) {
                // existing node, assume reboot
                node->reset(slots);

                ERS_DEBUG(1,"Resetting slots for existing node: " << id << " to " << slots);
            } else {
                // assume new node
                if(Port *port = Port::find(id)) {
                    Node *node = new Node(port, slots);
                    m_scheduler.add_node(node);
                    ERS_DEBUG(,1"Adding new node: " << id << " with " << slots << " slots");
                    m_stats.ProcessingNodesAdded++;

                } else {
                    // error: no such Port
                    ERS_LOG("Unknown message passing ID: " << id);
                }
            }
        }
    }
    

    void Activity::handle_lvl1_input()
    {
        while(m_running) {
            if(m_triggering) {
                if(dcmessages::LVL1Result * l1result = m_l1source->getResult()) {
                    m_incoming_events.put(new Event(l1result));
                    m_stats.LVL1Events++;
                    ERS_DEBUG(3,"Got new event: " << l1result->l1ID());
                }
            }
        }
    }

    void Activity::assign_event()
    {
        while(m_running) {
            Event *event = 0;
            if(m_incoming_events.get(event)) {
                if(Node *node = m_scheduler.select_node(event)) {

                    {
                        EventMap::accessor access;
                        m_events.insert(access, event->lvl1_id());
                        access->second = event;
                    }

                    ERS_DEBUG(3,"Assigned event " << event->lvl1_id() << " to node " << node->id());

                    m_stats.AssignedEvents++;

                    if(!event->l1result()->send(node->port(), 0)) {
                        // handle failed sending
                        node->disable();
                        node->free(event);

                        m_stats.ProcessingNodesDisabled++;

                        // re-allocate to other node...
                        ERS_LOG("Sending event " << event->lvl1_id() << " failed");
                    } else {
                        m_timeout_queue.put(event->lvl1_id());
                    }
                    
                } else {
                    // pretty bad...
                    // fatal()
                    ERS_LOG("No node available ! ");
                    delete event;
                    break;
                }
            } else {
                break;
            }
        }

        ERS_LOG("Events in input queue: " << m_incoming_events.size());
        m_incoming_events.clear(ProtectedQueue<Event*>::deleter());
    }

    void Activity::handle_decision()
    {
        RealTimeClock clock;

        while(m_running) {
            MessagePassing::Buffer *buffer;
            if(m_decision_queue.get(buffer)) {
                MessagePassing::Buffer::iterator it = buffer->begin();
                it += MessageInput::MessageHeader::SIZE;

                uint32_t lvl1_id;

                it >> lvl1_id;

                delete buffer;

                // ERS_LOG("Got decision for event " << lvl1_id);

                EventMap::accessor access;
                if(m_events.find(access, lvl1_id)) {
                    ERS_ASSERT_MSG(access->second != 0, "Invalid event pointer");
                    Event *event = access->second;
                    if(event->active()) {
                        event->done();
                        m_stats.ProcessedEvents++;
                        m_clear_queue.put(event);
                    } else {
                        ERS_LOG("Late reply for event" << lvl1_id);
                    }
                    Time diff = clock.time() - event->assigned();
                    m_time->Fill(diff.as_milliseconds());
                } else {
                    // oops, unknown lvl1 id ???
                    ERS_LOG("Unknown level 1 ID: " << lvl1_id);
                }
            } else {
                // wakeup
                break;
            }
        }

        m_decision_queue.clear(ProtectedQueue<MessagePassing::Buffer*>::deleter());
    }

    void Activity::handle_timeouts()
    {
        RealTimeClock clock;

        std::list<uint32_t> events;

        while(m_running) {

            usleep(10000);

            // get more events to check
            uint32_t event_id;
            while(m_timeout_queue.try_get(event_id)) {
                events.push_back(event_id);
            }

            daq::clocks::Time now = clock.time();

            for(std::list<uint32_t>::iterator it = events.begin(); it != events.end(); ) {
                
                EventMap::const_accessor access;
                if(m_events.find(access, (*it))) {
                    Event *event = access->second;
                
                    ERS_ASSERT_MSG(event != 0, "Invalid Event pointer");
                
                    // already been handled
                    if(!event->active()) {
                        it = events.erase(it);
                        continue;
                    }
                
                    if((now - event->assigned()).as_milliseconds() > 100000) {
                        // do something.
                        // but what ? There is no SFI to ask to build the event.
                        // If we re-assign there must be a way to tell the node that
                        // it should simply build the event, not process it.
                        ERS_LOG("Timeout for event " << event->lvl1_id());
                        
                        m_stats.Timeouts++;
                        event->done();
                        m_clear_queue.put(event);
                        it = events.erase(it);

                    } else {
                        ++it;
                    }
                } else {
                    // no longer in global map
                    it = events.erase(it);
                }
            }
        }
    }

    void Activity::handle_clears()
    {
        std::vector<LVL1ID> to_clear;

        while(m_running) {

            Event *event = 0;
            if(m_clear_queue.get(event)) {

                to_clear.push_back(event->lvl1_id());

                m_events.erase(event->lvl1_id());
                delete event;

                if(to_clear.size() >= 100) {  // or timeout...
                    ERS_DEBUG(3,"Sending clear with " << to_clear.size() << " events");
                    dcmessages::DFM_Clear_Msg msg(to_clear, 0, 0);
                    if(!msg.send(m_ros_group)) {
                        ERS_LOG("Problem sending clears");
                    }
                    to_clear.clear();
                }
            } else {
                // wakeup
                break;
            }
        }

        if(to_clear.size() > 0) {
            dcmessages::DFM_Clear_Msg msg(to_clear, 0, 0);
            msg.send(m_ros_group);
        }
    }

    void Activity::update_rates()
    {
        while(m_running) {
            uint64_t oldProcessedEvents = m_stats.ProcessedEvents;
            sleep(5);
            m_stats.Rate = (m_stats.ProcessedEvents - oldProcessedEvents)/5.0;
        }
    }

    /**
     *     MasterTrigger interface
     **/
    
    uint32_t Activity::hold()
    {
        m_triggerHoldCounter += 1;
        m_triggering = false;
        return 0;
    }

    void Activity::resume()
    {
        if (m_triggerHoldCounter > 0) {
            m_triggerHoldCounter -= 1;
        }

        if (m_triggerHoldCounter == 0) {
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

    void Activity::setPrescales(uint32_t  l1p, uint32_t hltp, uint32_t lb)
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
