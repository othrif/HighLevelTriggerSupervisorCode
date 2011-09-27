
#include "Activity.h"

#include <stdlib.h>

#include "dynlibs/DynamicLibrary.h"
#include "clocks/Clock.h"
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

#include "hltsvdal/HLTSVApplication.h"
#include "hltsvdal/HLTSVConfiguration.h"

#include "boost/function.hpp"

namespace hltsv {

    const size_t Num_Assign = 12;

    Activity::Activity()
        : m_incoming_events(64000),
          m_l1source_lib(0),
          m_l1source(0),
          m_resource(0),
          m_time(0),
          m_ros_group(0),
          m_network(false),
          m_running(false),
          m_triggering(false),
          m_num_assign_threads(Num_Assign),
          m_thread_input(0),
          m_thread_decision(0),
          m_thread_update_rates(0),
          m_triggerHoldCounter(0)
    {
    }

    Activity::~Activity()
    {
    }
        
    DC::StatusWord Activity::act_config()
    {

        AppControl *ac = AppControl::get_instance();

        Configuration* conf;
        ac->getConfiguration(&conf);

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



        if(!m_msgconf.configure(ac->getNodeID(), *conf)) {
            // fatal
            ERS_LOG("Cannot configure message passing");
            return DC::FATAL;
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
            m_cmdReceiver = new daq::rc::CommandedTrigger(IPCPartition(ac->getPartition()->UID()),
                                                          ac->getAppName(), this);
        }

        m_time = new TH1F("Time","Time", 2000, 0., 20000.);

        hltinterface::ITHistRegister::instance()->registerTObject("", "/DEBUG/HLTSV/Time", m_time);

        if(getenv("NUM_ASSIGN_THREADS")) {
            m_num_assign_threads = strtol(getenv("NUM_ASSIGN_THREADS"),0,0);
        }

        m_network = true;
        m_thread_decision = new boost::thread(&Activity::handle_network, this);

        return DC::OK;
    }

    DC::StatusWord Activity::act_connect()
    {
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

        for(size_t i = 0; i < m_num_assign_threads; i++) {
            m_thread_assign.push_back(new boost::thread(&Activity::assign_event, this));
        }

        m_thread_update_rates = new boost::thread(&Activity::update_rates, this);

        m_thread_input = new boost::thread(&Activity::handle_lvl1_input, this);

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

        m_incoming_events.wakeup();

        usleep(100000);

        m_thread_input->join();
        delete m_thread_input;

        for(size_t i = 0; i < m_num_assign_threads; i++) {
            m_thread_assign[i]->join();
            delete m_thread_assign[i];
        }
        m_thread_assign.clear();

        m_thread_update_rates->join();
        delete m_thread_update_rates;

        ERS_LOG("Events in input queue: " << m_incoming_events.size());
        m_incoming_events.clear(ProtectedQueue<Event*>::deleter());

        sleep(3);

        for(EventMap::iterator it = m_events.begin(); it != m_events.end(); ++it) {
            if(it->second->active()) { it->second->done(); }
            delete it->second;
        }

        m_events.clear();

        return DC::OK;
    }

    DC::StatusWord Activity::act_unconfig()
    {
        if(m_cmdReceiver) {
            m_cmdReceiver->_destroy();
            m_cmdReceiver = 0;
        }

        m_network = false;
        m_thread_decision->join();
        delete m_thread_decision;

        delete m_l1source;
        m_l1source = 0;
        m_l1source_lib->release();
        delete m_l1source_lib;

        m_msgconf.unconfigure();

        delete m_resource;
        m_resource = 0;

        // hltinterface::ITHistRegister::instance()->releaseTObject("", "/DEBUG/HLTSV/Time");
        // delete m_time;
        // m_time = 0;

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
        using namespace MessageInput;

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
                    if(!m_running) {
                        Node *node = new Node(port, slots);
                        m_scheduler.add_node(node);
                        ERS_DEBUG(1,"Adding new node: " << id << " with " << slots << " slots");
                        m_stats.ProcessingNodesAdded++;
                    } else {
                        ERS_LOG("Ignoring new node while running: " << id);
                    }
                } else {
                    // error: no such Port
                    ERS_LOG("Unknown message passing ID: " << id);
                }
            }
        }

        delete buffer;

    }
    

    void Activity::handle_lvl1_input()
    {
        ERS_LOG("Entering handle_lvl1_input");
        while(m_running) {
            if(m_triggering) {
                if(dcmessages::LVL1Result * l1result = m_l1source->getResult()) {
                    if(m_incoming_events.put(new Event(l1result))) {
                        m_stats.LVL1Events++;
                        ERS_DEBUG(3,"Got new event: " << l1result->l1ID());
                    } else {
                        ERS_LOG("handle_lvl1_input: wakeup");
                        delete l1result;
                        break;
                    }
                }
            } else {
                usleep(1000);
            }
        }
        ERS_LOG("Exiting handle_lvl1_input");
    }

    void Activity::assign_event()
    {
        RealTimeClock clock;

        while(m_running) {
            Event *event = 0;
            if(m_incoming_events.get(event)) {

                while(Node *node = m_scheduler.select_node(event)) {

                    ERS_DEBUG(3,"Assigned event " << event->lvl1_id() << " to node " << node->id());

                    m_stats.AssignedEvents++;

                    // enter in global table
                    EventMap::accessor access;
                    m_events.insert(access, event->lvl1_id());
                    access->second = event;

                    if(event->l1result()->send(node->port(), 0)) {

                        // success
                        break;
                    } else {

                        // get rid of event in table
                        m_events.erase(access);

                        // handle failed sending
                        node->disable();
                        node->free(event);

                        m_stats.ProcessingNodesDisabled++;

                        // re-allocate to other node...
                        ERS_LOG("Sending event " << event->lvl1_id() << "to node" << node->id() << " failed, re-allocating");
                    }
                        
                } // scheduler never return NULL
            }

        }

    }

    void Activity::handle_network()
    {
        static RealTimeClock clock;
        Time last_check = clock.time();

        ERS_LOG("Entering network handler");
        
        while(m_network) {

            if(MessagePassing::Buffer *buffer = MessagePassing::Port::receive(100000)) {

                MessageInput::MessageHeader header(buffer);

                if(!header.valid()) {
                    // log this
                    ERS_LOG("Invalid message header");
                    delete buffer;
                    continue;
                }

                switch(header.type()) {
                case 0x1234U:
                    handle_announce(buffer);
                    break;
                case 0x8765U:
                    handle_decision(buffer);
                    break;
                default:
                    ERS_LOG("Invalid message type: " << header.type());
                    delete buffer;
                    break;
                }
            }

            // timeout checking
            daq::clocks::Time now = clock.time();
            
            if((now - last_check).as_seconds() > 1.0) {
                
                for(EventMap::iterator it = m_events.begin(); it != m_events.end(); ) {
                    
                    EventMap::const_accessor access;
                    if(m_events.find(access, (*it).second->lvl1_id())) {
                        Event *event = access->second;
                        
                        ERS_ASSERT_MSG(event != 0, "Invalid Event pointer");
                        
                        // already been handled
                        if(!event->active()) {
                            ++it;
                            continue;
                        }
                        
                        Time diff = clock.time() - event->assigned();

                        if(diff.as_milliseconds() > 100000) {

                            // do something.
                            // but what ? There is no SFI to ask to build the event.
                            // If we re-assign there must be a way to tell the node that
                            // it should simply build the event, not process it.
                            ERS_LOG("Timeout for event " << event->lvl1_id() << " : " <<diff.seconds() << "." << diff.nano_seconds());
                            
                            m_stats.Timeouts++;
                            event->done();
                            ++it;
                            
                            m_events.erase(access);

                            uint32_t lvl1_id = event->lvl1_id();
                            delete event;
                            
                            add_event_to_clear(lvl1_id);
                        } else {
                            ++it;
                        }
                    } else {
                        ++it;
                    }
                }
                last_check = clock.time();
            }
        }

        ERS_LOG("Exiting network handler");
    }
                
    void Activity::handle_decision(MessagePassing::Buffer *buffer) 
    {
        static RealTimeClock clock;

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
                Time diff = clock.time() - event->assigned();
                m_time->Fill(diff.as_milliseconds());

                m_events.erase(access);
                delete event;
                
                add_event_to_clear(lvl1_id);
            } else {
                ERS_LOG("Late reply for event" << lvl1_id);
            }
        } else {
            // oops, unknown lvl1 id ???
            ERS_LOG("Unknown level 1 ID: " << lvl1_id);
        }
    }

    void Activity::add_event_to_clear(uint32_t lvl1_id)
    {
        m_to_clear.push_back(lvl1_id);

        if(m_to_clear.size() >= 100) {  // or timeout...
            ERS_DEBUG(3,"Sending clear with " << m_to_clear.size() << " events");
            dcmessages::DFM_Clear_Msg msg(m_to_clear, 0, 0);
            m_to_clear.clear();

            if(!msg.send(m_ros_group)) {
                ERS_LOG("Problem sending clears");
            }
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
