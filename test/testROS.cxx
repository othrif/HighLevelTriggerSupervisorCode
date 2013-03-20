#include "config/Configuration.h"

#include "cmdl/cmdargs.h"

#include "RunController/ConfigurationBridge.h"
#include "RunController/Controllable.h"
#include "RunController/ItemCtrl.h"

#include "asyncmsg/Server.h" 
#include "asyncmsg/Session.h" 
#include "asyncmsg/Message.h" 

#include "msgnamesvc/NameService.h"

#include "dal/Partition.h"
#include "DFdal/DFParameters.h"

#include <memory>

#include "ers/Assertion.h"

// The clear message
// After the common header we have:
//
// uint32_t sequence number
// uint32_t lvl1_ids[]
//
class ClearMessage : public daq::asyncmsg::InputMessage {
public:


    static const uint32_t ID = 0x12345678;

    ClearMessage(uint32_t size)
        : daq::asyncmsg::Message(),
          m_message(size)
    {
        // minimum is sequence number
        ERS_ASSERT_MSG(size >= sizeof(uint32_t), "ClearMessage too short, minimum is 4 bytes");
        ERS_ASSERT_MSG(size % sizeof(uint32_t) == 0, "ClearMessage not a multiple of 4 bytes");
    }

    std::uint32_t typeId() const  override
    {
        // don't care
        return 0;
    }

    std::uint32_t transactionId() const override
    {
        // don't care
        return 0;
    }

    void toBuffers(std::vector<boost::asio::mutable_buffer>& buffers) override
    {
        buffers.push_back(boost::asio::buffer(m_message));
    }

    // simple example user interface for message content

    uint32_t sequence_no() const
    {
        return m_message[0];
    }

    size_t num_clears() const
    {
        return m_message.size() - 1;
    }

    uint32_t operator[](size_t idx) const
    {
        ERS_ASSERT_MSG(idx < m_message.size() - 1, "Invalid index into ClearMessage");
        return m_message[idx + 1];
    }

private:
    std::vector<uint32_t> m_message;
};

//
// The Session class to receive the clears.
// We just print out the message.
//
class ClearSession : public daq::asyncmsg::Session {
public:
    ClearSession(boost::asio::io_service& service)
        : daq::asyncmsg::Session(service),
          m_last_sequence(0)
    {
    }

protected:

    // we are accepted, not opening ourselves
    void onOpen() noexcept override {}
    void onOpenError(const boost::system::error_code& error) noexcept override {}

    // we only expect a ClearMessage
    std::unique_ptr<daq::asyncmsg::InputMessage> 
    createMessage(std::uint32_t typeId,
                  std::uint32_t transactionId, std::uint32_t size) noexcept override
    {
        // ERS_ASSERT(typeId == ClearMessage::ID);
        return std::unique_ptr<ClearMessage>(new ClearMessage(size));
    }

    void onReceive(std::unique_ptr<daq::asyncmsg::InputMessage> message) override
    {
        std::unique_ptr<ClearMessage> msg(dynamic_cast<ClearMessage*>(message.release()));

        if(m_last_sequence + 1 != msg->sequence_no()) {
            std::cerr << "Unexpected sequence no, got: " << msg->sequence_no() << " expected: " << m_last_sequence+1 << std::endl;
        }

        m_last_sequence = msg->sequence_no();

        std::cout << "Got clear message, seq = " << msg->sequence_no() << '\n'
                  << "LVL1 IDs: " << msg->num_clears() << '\n';

        for(size_t i = 0; i < msg->num_clears(); i++) {
            std::cout << (*msg)[i] << ' ';
        }
        std::cout << std::endl;
    }

    void onReceiveError(const boost::system::error_code& error,
                        std::unique_ptr<daq::asyncmsg::InputMessage> message) noexcept override
    {
        // Issue error message
    }

    void onSend(std::unique_ptr<const daq::asyncmsg::OutputMessage> message) noexcept override
    {
        // never used
    }

    void onSendError(const boost::system::error_code& error,
                     std::unique_ptr<const daq::asyncmsg::OutputMessage> message) noexcept override
    {
        // never used
    }

private:
    uint32_t              m_last_sequence;


};

//
// The Server to accept incoming connections.
// There should be never more than one ClearSession since there's only one HLTSV...
//
class ClearServer : public daq::asyncmsg::Server {
public:

    ClearServer(boost::asio::io_service& service)
        : daq::asyncmsg::Server(service),
          m_session(new ClearSession(service))
    {
    }

    void onAccept(std::shared_ptr<daq::asyncmsg::Session> session) noexcept override
    {
        // fine, nothing to do
    }

    void onAcceptError(const boost::system::error_code& error,
                       std::shared_ptr<daq::asyncmsg::Session> session) noexcept override
    {
        // TODO: issue error message
    }

    void start() 
    {
        asyncAccept(std::shared_ptr<daq::asyncmsg::Session>(m_session));
    }

    void stop()
    {
        if(m_session) {
            m_session->close();
        }
    }

private:
    std::shared_ptr<ClearSession> m_session;
};

class ROSApplication : public daq::rc::Controllable {
public:
    ROSApplication(std::string& name)
        : daq::rc::Controllable(name), m_running(false)
    {
    }
    ~ROSApplication()
    {
    }

    virtual void initialize(std::string & ) override
    {
    }

    virtual void configure(std::string& ) override
    {
        daq::rc::ConfigurationBridge *cb = daq::rc::ConfigurationBridge::instance();
        Configuration *config = cb->getConfiguration();

        m_server.reset(new ClearServer(m_service));

        // CLEAR+appname
        m_server->listen("CLEAR");
        daq::msgnamesvc::NameService name_service(IPCPartition(cb->getPartition()->UID()),
                                                  config->cast<daq::df::DFParameters>(cb->getPartition()->get_DataFlowParameters())->get_DefaultDataNetworks());
        name_service.publish("CLEAR", m_server->localEndpoint().port());

    }

    virtual void unconfigure(std::string &) override
    {
    }
    
    virtual void connect(std::string& ) override
    {
    }

    virtual void stopL2SV(std::string &) override
    {
        m_running = false;
    }

    virtual void prepareForRun(std::string& ) override
    {
        m_running = true;
    }
  
  
  
private:
    bool                         m_running;
    boost::asio::io_service      m_service;
    std::unique_ptr<ClearServer> m_server;
};

int main(int argc, char *argv[])
{
    std::string name;
    std::string parent;
    bool interactive;
  
    CmdArgStr     app('N',"name", "name", "application name", CmdArg::isOPT);
    CmdArgStr     uniqueId('n',"uniquename", "uniquename", "unique application id", CmdArg::isREQ);
    CmdArgBool    iMode('i',"iMode", "turn on interactive mode", CmdArg::isOPT);
    CmdArgStr     segname('s',"segname", "segname", "segment name", CmdArg::isOPT);
    CmdArgStr     parentname('P',"parentname", "parentname", "parent name", CmdArg::isREQ);
  
    segname = "";
    parentname = "";
  
    CmdLine       cmd(*argv, &app, &uniqueId, &iMode, &segname, &parentname, NULL);
    CmdArgvIter   argv_iter(--argc, (const char * const *) ++argv);
  
    unsigned int status = cmd.parse(argv_iter);
    if (status) {
        cmd.error() << argv[0] << ": parsing errors occurred!" << std::endl ;
        exit(EXIT_FAILURE);
    }
    name = uniqueId;
    interactive = iMode;
    parent = parentname;
  
    daq::rc::ItemCtrl control(new ROSApplication(name), interactive, parent);
    control.run();
}
