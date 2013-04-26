#include "config/Configuration.h"

#include "cmdl/cmdargs.h"

#include "RunController/ConfigurationBridge.h"
#include "RunController/Controllable.h"
#include "RunController/ItemCtrl.h"

#include "asyncmsg/Server.h" 
#include "asyncmsg/Session.h" 
#include "asyncmsg/Message.h" 
#include "asyncmsg/NameService.h"
#include "asyncmsg/UDPSession.h"

#include "dal/Partition.h"
#include "dal/RunControlApplicationBase.h"
#include "DFdal/DFParameters.h"

#include <memory>
#include <thread>

#include "ers/ers.h"

#include "is/infoT.h"
#include "is/infodictionary.h"


namespace {
    uint64_t              num_clears;
}


// The clear message
// After the common header we have:
//
// uint32_t sequence number
// uint32_t num_lvl1_ids
// uint32_t lvl1_ids[num_lvl1_ids]
//
class ClearMessage : public daq::asyncmsg::InputMessage {
public:


    static const uint32_t ID = 0x00DCDF10;

    ClearMessage(uint32_t size, uint32_t transactionId)
        : daq::asyncmsg::Message(),
          m_message(size/sizeof(uint32_t)),
          m_transactionId(transactionId)
    {
        // minimum is sequence number
        ERS_ASSERT_MSG(size >= 2 * sizeof(uint32_t), "ClearMessage too short, minimum is 4 bytes");
        ERS_ASSERT_MSG(size % sizeof(uint32_t) == 0, "ClearMessage not a multiple of 4 bytes");
    }

    std::uint32_t typeId() const  override
    {
        return ID;
    }

    std::uint32_t transactionId() const override
    {
        return m_transactionId;
    }

    void toBuffers(std::vector<boost::asio::mutable_buffer>& buffers) override
    {
        buffers.push_back(boost::asio::buffer(m_message));
    }

    // simple example user interface for message content

    uint32_t sequence_no() const
    {
        return m_transactionId;
    }

    size_t num_clears() const
    {
        return m_message.size();
    }

    uint32_t operator[](size_t idx) const
    {
        return m_message.at(idx);
    }

private:
    std::vector<uint32_t> m_message;
    uint32_t              m_transactionId;
};

//
// The Session class to receive the clears.
// We just print out the message.
//
class ClearSession : public daq::asyncmsg::Session {
public:
    ClearSession(boost::asio::io_service& service)
        : daq::asyncmsg::Session(service),
          m_expected_sequence(0)
    {
    }

protected:

    void onOpen() noexcept override 
    {
        ERS_LOG("Session is open");
        asyncReceive();
    }

    void onOpenError(const boost::system::error_code& error) noexcept override 
    {
        ERS_LOG("Open error..." << error);
    }

    // we only expect a ClearMessage
    std::unique_ptr<daq::asyncmsg::InputMessage> 
    createMessage(std::uint32_t typeId,
                  std::uint32_t transactionId, std::uint32_t size) noexcept override
    {
        ERS_ASSERT_MSG(typeId == ClearMessage::ID, "Unexpected type ID in message: " << typeId);
        ERS_DEBUG(3,"createMessage, size = " << size);
        return std::unique_ptr<ClearMessage>(new ClearMessage(size, transactionId));
    }

    void onReceive(std::unique_ptr<daq::asyncmsg::InputMessage> message) override
    {
        ERS_DEBUG(3,"onReceive");
        std::unique_ptr<ClearMessage> msg(dynamic_cast<ClearMessage*>(message.release()));

        if(m_expected_sequence != msg->sequence_no()) {
            ERS_LOG("Unexpected sequence no, got: " << msg->sequence_no() << " expected: " << m_expected_sequence);
        }

        m_expected_sequence = msg->sequence_no() + 1;

        ERS_DEBUG(1,"Got clear message, seq = " << msg->sequence_no() << " number ofLVL1 IDs: " << msg->num_clears());

        num_clears += msg->num_clears();

        asyncReceive();
    }

    void onReceiveError(const boost::system::error_code& error,
                        std::unique_ptr<daq::asyncmsg::InputMessage> /* message */) noexcept override
    {
         ERS_LOG("Receive error: " << error);
        // Issue error message
    }

    void onSend(std::unique_ptr<const daq::asyncmsg::OutputMessage> /* message */) noexcept override
    {
        // never used
        ERS_LOG("Should never happen");
    }

    void onSendError(const boost::system::error_code& error,
                     std::unique_ptr<const daq::asyncmsg::OutputMessage> /* message */) noexcept override
    {
        // never used
        ERS_LOG("Should never happen: " << error);
    }

private:
    uint32_t              m_expected_sequence;
};

class UDPClearSession : public daq::asyncmsg::UDPSession {
public:


    UDPClearSession(boost::asio::io_service& service, const std::string& multicast_address, const std::string& outgoing)
        : daq::asyncmsg::UDPSession(service, 9000),
          m_expected_sequence(0)
    {
        joinMulticastGroup(boost::asio::ip::address::from_string(multicast_address), daq::asyncmsg::NameService::find_interface(outgoing));
    }
    
    // we only expect a ClearMessage
    std::unique_ptr<daq::asyncmsg::InputMessage> 
    createMessage(std::uint32_t typeId,
                  std::uint32_t transactionId, std::uint32_t size) noexcept override
    {
        ERS_ASSERT_MSG(typeId == ClearMessage::ID, "Unexpected type ID in message: " << typeId);
        ERS_DEBUG(3,"createMessage, size = " << size);
        return std::unique_ptr<ClearMessage>(new ClearMessage(size, transactionId));
    }

    void onReceive(std::unique_ptr<daq::asyncmsg::InputMessage> message) override
    {
        ERS_DEBUG(3,"onReceive");
        std::unique_ptr<ClearMessage> msg(dynamic_cast<ClearMessage*>(message.release()));

        if(m_expected_sequence != msg->sequence_no()) {
            ERS_LOG("Unexpected sequence no, got: " << msg->sequence_no() << " expected: " << m_expected_sequence);
        }

        m_expected_sequence = msg->sequence_no() + 1;

        ERS_DEBUG(1,"Got clear message, seq = " << msg->sequence_no() << " number ofLVL1 IDs: " << msg->num_clears());

        num_clears += msg->num_clears();

        asyncReceive();
    }

    void onReceiveError(const boost::system::error_code& error,
                        std::unique_ptr<daq::asyncmsg::InputMessage> /* message */) noexcept override
    {
        ERS_LOG("Receive error: " << error);
        // Issue error message
    }

    void onSend(std::unique_ptr<const daq::asyncmsg::OutputMessage> /* message */) noexcept override
    {
        // never used
        ERS_LOG("should never happen");
    }

    void onSendError(const boost::system::error_code& error,
                     std::unique_ptr<const daq::asyncmsg::OutputMessage> /* message */) noexcept override
    {
        // never used
        ERS_LOG("should never happen" << error);
    }

private:
    uint32_t              m_expected_sequence;
};

//
// The Server to accept incoming connections.
// There should be never more than one ClearSession since there's only one HLTSV...
//
class ClearServer : public daq::asyncmsg::Server {
public:

    ClearServer(boost::asio::io_service& service)
        : daq::asyncmsg::Server(service),
          m_service(service)
    {
    }

    void onAccept(std::shared_ptr<daq::asyncmsg::Session> session) noexcept override
    {
        ERS_LOG("Accepted connection from "<< session->remoteEndpoint());
        m_session = session;
    }

    void onAcceptError(const boost::system::error_code& error,
                       std::shared_ptr<daq::asyncmsg::Session> /* session */) noexcept override
    {
        ERS_LOG("Accept error: " << error);
    }

    void start() 
    {
        auto session = std::make_shared<ClearSession>(m_service);
        asyncAccept(session);
    }

    void stop()
    {
        if(m_session) {
            m_session->close();
            m_session.reset();
        }
    }

private:
    boost::asio::io_service&                 m_service;
    std::shared_ptr<daq::asyncmsg::Session>  m_session;
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

    virtual void configure(std::string& ) override
    {
        daq::rc::ConfigurationBridge *cb = daq::rc::ConfigurationBridge::instance();
        Configuration *config = cb->getConfiguration();

        const daq::df::DFParameters *dfparams = config->cast<daq::df::DFParameters>(cb->getPartition()->get_DataFlowParameters());

        m_dict.reset(new ISInfoDictionary(IPCPartition(cb->getPartition()->UID())));

        if(dfparams->get_MulticastAddress().empty()) {

            // TCP version
            
            m_server.reset(new ClearServer(m_service));
            
            // CLEAR+appname
            m_server->listen("CLEAR_" + getName());
            daq::asyncmsg::NameService name_service(IPCPartition(cb->getPartition()->UID()),
                                                    dfparams->get_DefaultDataNetworks());
            name_service.publish("CLEAR_" + getName(), m_server->localEndpoint().port());
            
            m_server->start();

        } else {
            // UDP version

            // hack, parsing entry
            auto mc = dfparams->get_MulticastAddress();
            auto n = mc.find('/');
            auto mcast = mc.substr(0, n);
            auto outgoing = mc.substr(n+1);

            ERS_LOG("Configuring for multicast: " << mcast);

            m_udp_session = std::make_shared<UDPClearSession>(m_service, mcast,outgoing);
            m_udp_session->asyncReceive();

        }
        

        m_io_thread = std::thread([&]() { m_service.run(); ERS_LOG("io_service finished"); });

    }

    virtual void unconfigure(std::string &) override
    {
        if(m_server) {
            m_server->stop();
        } // else stop UDPsession
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

    virtual void probe(std::string& ) override
    {
        ISInfoUnsignedLong value;
        value.setValue(num_clears);
        m_dict->checkin( "DF." + getName(), value);
    }
  
private:
    std::thread                  m_io_thread;
    bool                         m_running;
    boost::asio::io_service      m_service;
    std::shared_ptr<ClearServer> m_server;
    std::shared_ptr<UDPClearSession> m_udp_session;
    std::unique_ptr<ISInfoDictionary> m_dict;
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
