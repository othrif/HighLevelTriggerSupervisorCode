
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
        : m_port(0),
          m_running(false)
    {
    }

    ~DCMActivity()
    {
    }

    DC::StatusWord act_config()
    {
        Configuration *config = 0;
        AppControl::get_instance()->getConfiguration(&config);
        m_msgconf.configure(AppControl::get_instance()->getNodeID(), *config);

        m_port = m_msgconf.create_by_group("L2SV").front();

        return DC::OK;
    }

    DC::StatusWord act_connect()
    {
        MessagePassing::Buffer announce(128);

        MessageInput::MessageHeader header(0x12345678, 0, MessagePassing::Port::self(),
                                           MessageInput::MessageHeader::SIZE + sizeof(uint32_t));
                        
        MessagePassing::Buffer::iterator it = announce.begin();
        it << header << 8; // hard-coded number of "worker" processses

        announce.size(it);

        m_port->send(&announce, true);
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
        return DC::OK;
    }

    void execute()
    {
        MessagePassing::Buffer reply(128);

        while(m_running) {

            if(MessagePassing::Buffer *buf = MessagePassing::Port::receive(100000)) {

                dcmessages::LVL1Result l1result(buf);
                
                delete buf;

                // ERS_LOG("Got event " << l1result.l1ID());

                MessageInput::MessageHeader header(0x87654321,
                                                   0, 
                                                   MessagePassing::Port::self(), 
                                                   MessageInput::MessageHeader::SIZE + sizeof(uint32_t));
                
                MessagePassing::Buffer::iterator it = reply.begin();
                it << header << l1result.l1ID();

                reply.size(it);
                m_port->send(&reply, true);

                // ERS_LOG("Sent reply " << l1result.l1ID());
                
            }
        }
    }

private:
    MessageConfiguration m_msgconf;
    MessagePassing::Port *m_port;
    bool                 m_running;
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

