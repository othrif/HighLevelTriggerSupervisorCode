
#include "ac/AppControl.h"
#include "ac/ThreadedUserActivity.h"
#include "ac/parse_cmd.h"

#include "config/Configuration.h"

#include "msg/Port.h"
#include "msgconf/MessageConfiguration.h"
#include "msginput/MessageHeader.h"

#include "dcmessages/LVL1Result.h"

#include "queues/ProtectedQueue.h"

#include <boost/thread/thread.hpp>
#include <tbb/atomic.h>
#include <cstdlib>

class DCMActivity : public ThreadedUserActivity {
public:
    DCMActivity()
        : m_running(false),
          m_cores(8)
    {
        if(getenv("NUM_CORES") != 0) {
            m_cores = strtoul(getenv("NUM_CORES"), 0, 0);
        }
    }

    ~DCMActivity()
    {
    }

    DC::StatusWord act_config()
    {
        Configuration *config = 0;
        AppControl::get_instance()->getConfiguration(&config);

        if(!m_msgconf.configure(AppControl::get_instance()->getNodeID(), *config)) {
            ERS_LOG("Cannot configure message passing");
            return DC::FATAL;
        }

        m_ports = m_msgconf.create_by_group("HLTSV");


        if(getenv("DCM_L2_PROCESSING")) {
            m_l2_processing = strtoul(getenv("DCM_L2_PROCESSING"), 0, 0);
        } else {
            m_l2_processing = 40;
        }

        if(getenv("DCM_L2_ACCEPT")) {
            m_l2_accept = strtoul(getenv("DCM_L2_ACCEPT"), 0, 0);
        } else {
            m_l2_accept = 5;
        }

        if(getenv("DCM_EVENT_BUILDING")) {
            m_event_building = strtoul(getenv("DCM_EVENT_BUILDING"), 0, 0);
        } else {
            m_event_building = 40;
        }

        if(getenv("DCM_EVENT_FILTER")) {
            m_event_filter = strtoul(getenv("DCM_EVENT_FILTER"), 0, 0);
        } else {
            m_event_filter = 2;
        }

        ERS_LOG("l2: " << m_l2_processing << " l2_acc: " << m_l2_accept << 
                " eb: " << m_event_building << " ef: " << m_event_filter);

        
        return DC::OK;
    }

    DC::StatusWord act_connect()
    {
#if 0
        MessagePassing::Buffer announce(128);

        MessageInput::MessageHeader header(0x8765, 0, MessagePassing::Port::self(),
                                           MessageInput::MessageHeader::SIZE + sizeof(uint32_t));
                        
        MessagePassing::Buffer::iterator it = announce.begin();
        it << header << (uint32_t)0 << m_cores; 

        announce.size(it);

        m_ports.front()->send(&announce, true);
#endif

        return DC::OK;
    }

    DC::StatusWord act_prepareForRun()
    {
        m_outstanding = 0;

        m_running = true;

        for(size_t i = 0; i < m_cores; i++) {
            m_handlers.push_back(new boost::thread(&DCMActivity::event_handler, this));
        }

        this->start();
        return DC::OK;
    }

    DC::StatusWord act_stopL2()
    {
        m_running = false;
        this->stop();
        this->wait();
        m_queue.wakeup();
        for(size_t i = 0; i < m_cores; i++) {
            m_handlers[i]->join();
        }

        m_handlers.clear();
        m_queue.clear();
        
        return DC::OK;
    }

    DC::StatusWord act_unconfig()
    {
        m_msgconf.unconfigure();
        m_ports.clear();
        return DC::OK;
    }

    void execute()
    {

        using namespace MessagePassing;
        using namespace MessageInput;

        while(m_running) {

            if(Buffer *buf = Port::receive(100000)) {

                MessageHeader input(buf);
                if(!input.valid()) {
                    delete buf;
                    continue;
                }

                dcmessages::LVL1Result *l1result = new dcmessages::LVL1Result(buf);

                m_queue.put(l1result);
                
                delete buf;
            }
        }

    }

    void event_handler()
    {
        using namespace MessagePassing;
        using namespace MessageInput;

        Buffer reply(128);

        srand(Port::self());

        while(m_running) {

            dcmessages::LVL1Result *l1result = 0;
            if(m_queue.get(l1result)) {

                // ERS_LOG("Got event " << l1result.l1ID());

                usleep(m_l2_processing * 1000);

                MessageHeader header(0x8765U,
                                     0, 
                                     Port::self(), 
                                     MessageHeader::SIZE + 3 * sizeof(uint32_t));
                
                if((uint32_t)rand() % 100 >  m_l2_accept) {

                    // "LVL2 reject"
                    // reply with 1 x L1ID, 1 slot to add

                    uint32_t update = m_outstanding.fetch_and_store(0) + 1;

                    Buffer::iterator it = reply.begin();
                    it << header << (uint32_t)1 << l1result->l1ID() << update;
                    reply.size(it);

                    m_ports.front()->send(&reply, true);


                }  else {

                    // "event building"
                    usleep(m_event_building * 1000);

                    // reply with 1 x L1ID, 0 slots to add

                    uint32_t update = m_outstanding.fetch_and_store(0);

                    Buffer::iterator it = reply.begin();
                    it << header << (uint32_t)1 << l1result->l1ID() << update;
                    reply.size(it);

                    m_ports.front()->send(&reply, true);

                    // "event filter"
                    sleep(m_event_filter);

                    // add another update slot
                    update = m_outstanding.fetch_and_increment() + 1;

                    if(update == m_cores) {

                        // This means all cores where busy with EF processing and
                        // nobody will be replying. Have to send a dummy message
                        // to get out of this.

                        m_outstanding = 0;

                        // reply with 0 x L1ID, m_core slots to add
                        Buffer::iterator it = reply.begin();

                        it << header << (uint32_t)0 << update;
                        reply.size(it);

                        m_ports.front()->send(&reply, true);
                        
                    }
                    
                }

                delete l1result;

            } else {
                // wakeup -> finish
                break;
            }
        }
    }

private:
    MessageConfiguration             m_msgconf;
    std::list<MessagePassing::Port*> m_ports;
    bool                             m_running;
    ProtectedQueue<dcmessages::LVL1Result*> m_queue;
    std::vector<boost::thread*>      m_handlers;
    tbb::atomic<uint32_t>            m_outstanding;
    uint32_t                         m_cores;

    uint32_t                         m_l2_processing;
    uint32_t                         m_l2_accept;
    uint32_t                         m_event_building;
    uint32_t                         m_event_filter;
};

int main(int argc, char *argv[])
{
    cmdargs cmdLineArgs = parse_cmd("DCM Application", argc, argv);
    AppControl* ac = AppControl::get_instance(cmdLineArgs);

    DCMActivity dcm;

    ac->registerActivity(&dcm);
    ac->run();
    ac->unregisterActivity(&dcm);    

    return 0;
}

