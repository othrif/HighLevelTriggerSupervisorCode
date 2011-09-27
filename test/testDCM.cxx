
#include "ac/AppControl.h"
#include "ac/ThreadedUserActivity.h"
#include "ac/parse_cmd.h"

#include "config/Configuration.h"

#include "msg/Port.h"
#include "msgconf/MessageConfiguration.h"
#include "msginput/MessageHeader.h"

#include "dcmessages/LVL1Result.h"

class DCMActivity : public ThreadedUserActivity {
public:
    DCMActivity()
        : m_running(false)
    {
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

        m_ports = m_msgconf.create_by_group("L2SV");
        
        return DC::OK;
    }

    DC::StatusWord act_connect()
    {
        MessagePassing::Buffer announce(128);

        MessageInput::MessageHeader header(0x1234, 0, MessagePassing::Port::self(),
                                           MessageInput::MessageHeader::SIZE + sizeof(uint32_t));
                        
        MessagePassing::Buffer::iterator it = announce.begin();
        it << header << 8; // hard-coded number of "worker" processses

        announce.size(it);

        m_ports.front()->send(&announce, true);

        return DC::OK;
    }

    DC::StatusWord act_prepareForRun()
    {
        m_running = true;
        this->start();
        return DC::OK;
    }

    DC::StatusWord act_stopL2()
    {
        m_running = false;
        this->stop();
        this->wait();
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

        Buffer reply(128);

        while(m_running) {

            if(Buffer *buf = Port::receive(100000)) {

                MessageHeader input(buf);
                if(!input.valid()) {
                    delete buf;
                    continue;
                }

                dcmessages::LVL1Result l1result(buf);
                
                delete buf;

                // ERS_LOG("Got event " << l1result.l1ID());

                MessageHeader header(0x8765U,
                                     0, 
                                     Port::self(), 
                                     MessageHeader::SIZE + sizeof(uint32_t));
                
                Buffer::iterator it = reply.begin();
                it << header << l1result.l1ID();

                reply.size(it);

                if(Port *port = Port::find(input.source())) {
                    port->send(&reply, true);
                } else {
                    ERS_LOG("Invalid source node ID: " << input.source());
                }

                // ERS_LOG("Sent reply " << l1result.l1ID());
                
            }
        }
    }

private:
    MessageConfiguration             m_msgconf;
    std::list<MessagePassing::Port*> m_ports;
    bool                             m_running;
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

