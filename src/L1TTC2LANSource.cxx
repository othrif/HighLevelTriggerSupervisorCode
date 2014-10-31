
#include <vector>
#include <list>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>

#include "ers/ers.h"

#include "eformat/eformat.h"
#include "eformat/write/eformat.h"

#include "LVL1Result.h"
#include "L1Source.h"
#include "Issues.h"

#include "config/Configuration.h"
#include "DFdal/RoIBPluginTTC2LAN.h"

#include "asyncmsg/Message.h"
#include "asyncmsg/Session.h"
#include "asyncmsg/Server.h"
#include "asyncmsg/NameService.h"

namespace {


    // Don't know the size of a boolean, but
    // we can control the size of an enum now

    enum class XONOFFStatus : uint32_t { OFF = 0, ON = 1 };

    /**
     * Change XON/XOFF status. Used to throttle the send.
     */
    class XONOFF : public daq::asyncmsg::OutputMessage {
    public:

        static const uint32_t ID = 0x00DCDF50;
        
        XONOFF(XONOFFStatus on_off)
            : m_on_off(on_off)
        {}
        
        uint32_t typeId() const override 
        {
            return ID;
        }

        uint32_t transactionId() const override
        {
            return 0;
        }

        void toBuffers(std::vector<boost::asio::const_buffer>& buffers) const override
        {
            buffers.push_back(boost::asio::buffer(&m_on_off, sizeof(m_on_off)));
        }

    private:
        XONOFFStatus m_on_off;

    };

    /**
     * Update received from TTC2LAN about next event range.
     */
    class Update : public daq::asyncmsg::InputMessage {
    public:

        static const uint32_t ID = 0x00DCDF51;

        uint32_t typeId() const override 
        {
            return ID;
        }

        uint32_t transactionId() const override
        {
            return 0;
        }

        void toBuffers(std::vector<boost::asio::mutable_buffer>& buffers) override
        {
            buffers.push_back(boost::asio::buffer(&m_l1_id, sizeof(m_l1_id)));
        }

        uint32_t l1_id() const
        {
            return m_l1_id;
        }

    private:

        uint32_t m_l1_id;

    };

    class Session;

    //
    // This abstract interface is implemented by our L1TTC2LANSource class to receive feedback
    // when a message is received.
    //
    class Callback {
    public:
        virtual ~Callback() {}
        virtual void insert(uint32_t first, uint32_t last) = 0;
        virtual void connected(std::shared_ptr<daq::asyncmsg::Session> session) = 0;
    };
    
    class Session : public daq::asyncmsg::Session {
    public:
        explicit Session(boost::asio::io_service& service, Callback& cb)
            : daq::asyncmsg::Session(service),
              m_last_id(0xff000000),
              m_callback(cb),
              m_ECRcnt(0),
              m_debug(false)
        {
        }

        Session(const Session& ) = delete;
        Session& operator=(const Session& ) = delete;

    protected:
        void onOpen() noexcept override
        {
            ERS_LOG("Connection from " << remoteEndpoint());
            asyncReceive();
        }

        void onOpenError(const boost::system::error_code& error) noexcept override
        {
            ERS_LOG("Error during open: " << error);
        }

        std::unique_ptr<daq::asyncmsg::InputMessage> createMessage(uint32_t typeId,
                                                                   uint32_t /* transactionId */,
                                                                   uint32_t size) noexcept override
        {
            if((typeId != Update::ID) || (size != sizeof(uint32_t))) {
                // indicates an error
                return std::unique_ptr<daq::asyncmsg::InputMessage>();
            }
            
            return std::unique_ptr<daq::asyncmsg::InputMessage>(new Update);
        }

        void onReceive(std::unique_ptr<daq::asyncmsg::InputMessage> message) override
        {
            auto msg = std::unique_ptr<Update>(dynamic_cast<Update*>(message.release()));

            if(msg == 0) {
                ERS_LOG("Received invalid message type " << msg->typeId());
                return;
            }

            uint32_t l1id = msg->l1_id();
          
            if (m_debug) ERS_LOG("TTC2LAN: recieved new L1ID  "<<l1id<<" in hex: "<<std::hex<<l1id);
                               
            if((m_last_id & 0xff000000) == (l1id & 0xff000000)) {
                // no ECR happened
                m_callback.insert(m_last_id + 1, l1id);
            } else {
                // ECR happened
              m_ECRcnt++;
                if (m_debug) ERS_LOG("TTC2LAN: ECR detected "<<l1id<<" in hex: "<<std::hex<<l1id);
                m_callback.insert(l1id & 0xff000000, l1id);
            }

            m_last_id = l1id;

            asyncReceive();

        }

        void onReceiveError(const boost::system::error_code& error,
                            std::unique_ptr<daq::asyncmsg::InputMessage> /* message */) noexcept override
        {
            ERS_LOG("Receive error: " << error);
            asyncClose();
        }


        void onSend(std::unique_ptr<const daq::asyncmsg::OutputMessage> /* message */) noexcept override
        {
            // message is deleted automatically
        }

        void onSendError(const boost::system::error_code& error,
                         std::unique_ptr<const daq::asyncmsg::OutputMessage> /* message */) noexcept override
        {
            ERS_LOG("Send error: " << error);
            asyncClose();
        }

        void onClose() noexcept override
        {}
        
        void onCloseError(const boost::system::error_code& error) noexcept override
        {
            ERS_LOG("Close error: " << error);
        }

    private:
        uint32_t  m_last_id;
        Callback& m_callback;
        uint32_t m_ECRcnt;
        bool m_debug;
    };

    class Server : public daq::asyncmsg::Server {
    public:
        Server(boost::asio::io_service& service,
               Callback& cb,
               const std::vector<std::string>& networks)
            : daq::asyncmsg::Server(service),
              m_service(service),
              m_callback(cb),
              m_networks(networks)
        {
        }

        ~Server()
        {}

        void start()
        {
            listen("HLTSV");

            m_session = std::make_shared<Session>(m_service, m_callback);
            asyncAccept(m_session);

            daq::asyncmsg::NameService name_service(IPCPartition(getenv("TDAQ_PARTITION")), m_networks);
            name_service.publish("TTC2LANReceiver", localEndpoint().port());
        }

    protected:

        void onAccept(std::shared_ptr<daq::asyncmsg::Session> session) noexcept override
        {
            ERS_LOG("Accepted connection from " << session->remoteEndpoint());
            m_callback.connected(session);
            m_session = std::make_shared<Session>(m_service, m_callback);
            asyncAccept(m_session);
        }

        void onAcceptError(const boost::system::error_code& error,
                           std::shared_ptr<daq::asyncmsg::Session> /* session */) noexcept override
        {
            // TODO: check for aborted
            
            // report error
            ERS_LOG("Accept error: " << error);
        }

    private:
        boost::asio::io_service& m_service;
        Callback&                m_callback;
        std::vector<std::string> m_networks;

        std::shared_ptr<Session> m_session;
    };


}

namespace hltsv {

    /**
     * \brief The L1TTC2LANSource class encapsulates the version of the
     * L1Source class which provides a LVL1Result object internally
     * from configuration parameters.
     *
     */
    class L1TTC2LANSource : public L1Source, public Callback {
    public:
        L1TTC2LANSource(const daq::df::RoIBPluginTTC2LAN *config);
        ~L1TTC2LANSource();

        // L1Source interface
        LVL1Result* getResult() override;
        void        reset(uint32_t run_number) override;
        void        preset() override;

        // Callback interface
        void insert(uint32_t first, uint32_t last) override;
        void connected(std::shared_ptr<daq::asyncmsg::Session> session) override;

    private:
        std::vector<std::string> m_networks;
        uint32_t                 m_high_watermark;
        uint32_t                 m_low_watermark;
        std::vector<uint32_t>    m_dummy_data;

        // For the message passing
        boost::asio::io_service m_io_service;
        std::unique_ptr<boost::asio::io_service::work> m_work;
        std::shared_ptr<Server> m_server;

        std::mutex                              m_mutex;
        std::list<std::pair<uint32_t,uint32_t>> m_events;
        uint32_t                                m_event_count;
        XONOFFStatus                            m_status;
        bool                                    m_debug;

        std::shared_ptr<daq::asyncmsg::Session> m_session;

        std::thread                             m_io_thread;
    };
}

extern "C" hltsv::L1Source *create_source(Configuration *config, const daq::df::RoIBPlugin *roib, const std::vector<std::string>& /* unused */)
{

    const daq::df::RoIBPluginTTC2LAN *my_config = config->cast<daq::df::RoIBPluginTTC2LAN>(roib);
    if(my_config == nullptr) {
        throw hltsv::ConfigFailed(ERS_HERE, "Invalid type for configuration to L1TTC2LANSource");
    }
    return new hltsv::L1TTC2LANSource(my_config);
}

namespace hltsv {

    L1TTC2LANSource::L1TTC2LANSource(const daq::df::RoIBPluginTTC2LAN *config)
        : m_networks(config->get_Networks()),
          m_high_watermark(config->get_High_Watermark()),
          m_low_watermark(config->get_Low_Watermark()),          
          m_dummy_data(5),
          m_event_count(0),
          m_status(XONOFFStatus::ON),
          m_debug(false)
    {
    }

    L1TTC2LANSource::~L1TTC2LANSource()
    {
        m_work.reset();
        m_io_service.stop();
        m_io_thread.join();
    }

    LVL1Result* L1TTC2LANSource::getResult()
    {
      
      if (m_debug) {ERS_LOG("L1TTC2LANSource::getResult(): m_event_count = "<<m_event_count
              <<" m_low_watermark = "<<m_low_watermark
              <<" m_status = "<<bool(m_status == XONOFFStatus::ON)
              <<" l1ids in list: "<<m_events.size()
                          );}
       
        if(m_status == XONOFFStatus::OFF && m_event_count < m_low_watermark) {
          if (m_debug) { ERS_LOG("L1TTC2LANSource::getResult(): sending XON"); }
            std::unique_ptr<daq::asyncmsg::OutputMessage> msg(new XONOFF(XONOFFStatus::ON));
            m_session->asyncSend(std::move(msg));
            m_status = XONOFFStatus::ON;
        }

        uint32_t l1id;

        { // lock
            std::unique_lock<std::mutex> lock(m_mutex);
                
            if(m_events.empty()) {
                return 0;
            }
            
            l1id = m_events.front().first;
            if(++m_events.front().first > m_events.front().second) {
                m_events.pop_front();
            }

            m_event_count--;
        } // end of lock

        //create the ROB fragment 
        const uint32_t bc_id	 = 0x1;
        const uint32_t lvl1_type = 0xff;
  
        const uint32_t event_type = 0x0; 

        eformat::helper::SourceIdentifier src(eformat::TDAQ_CTP, 1);

        eformat::write::ROBFragment rob(src.code(), m_run_number, l1id, bc_id,
                                        lvl1_type, event_type, 
                                        m_dummy_data.size(), &m_dummy_data[0], 
                                        eformat::STATUS_FRONT);

        auto fragment = new uint32_t[rob.size_word()];
        eformat::write::copy(*rob.bind(), fragment, rob.size_word());

        LVL1Result* l1Result = new LVL1Result(l1id, fragment, rob.size_word());

        return l1Result;
    }

    void L1TTC2LANSource::insert(uint32_t first, uint32_t last)
    {
        { // lock
            std::unique_lock<std::mutex> lock(m_mutex);
            m_events.push_back(std::make_pair(first,last));
            m_event_count += (last - first + 1);
        } // end of lock
      
        if(m_debug) {ERS_LOG("L1TTC2LANSource::insert(): first = "<<first<<" last = "<<last<<" new m_event_count = "<<m_event_count);}
        if(m_event_count > m_high_watermark && m_status == XONOFFStatus::ON) {
            std::unique_ptr<daq::asyncmsg::OutputMessage> msg(new XONOFF(XONOFFStatus::OFF));
            m_session->asyncSend(std::move(msg));
            m_status = XONOFFStatus::OFF;
            ERS_LOG("L1TTC2LANSource::insert(): sending XOFF because m_event_count:  "<<m_event_count<<" > m_high_watermark = "<<m_high_watermark);
        }
    }

    void L1TTC2LANSource::connected(std::shared_ptr<daq::asyncmsg::Session> session)
    {
        m_session = session;
    }

    void L1TTC2LANSource::preset()
    {
        m_work.reset(new boost::asio::io_service::work(m_io_service));

        m_events.clear();
        m_event_count = 0;
        m_server = std::make_shared<Server>(m_io_service,
                                            *this,
                                            m_networks);
        m_server->start();
        m_io_thread = std::thread([&] { m_io_service.run(); });
    }

    void
    L1TTC2LANSource::reset(uint32_t run_number)
    {
        m_events.clear();
        m_event_count = 0;
        m_run_number = run_number;
    }
    

}
