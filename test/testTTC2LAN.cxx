#include "config/Configuration.h"

#include "cmdl/cmdargs.h"

#include "RunControl/Common/OnlineServices.h"
#include "RunControl/Common/Controllable.h"
#include "RunControl/Common/CmdLineParser.h"
#include "RunControl/ItemCtrl/ItemCtrl.h"

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
#include <atomic>

#include "ers/ers.h"

#include "is/infoT.h"
#include "is/infodictionary.h"


namespace { 

    // The current state of XON/XOFF from the HLT supervisor.
    // We don't know the size of a boolean, but we can control the size of an enum now
    // in C++11

    enum class XONOFFStatus : uint32_t { OFF = 0, ON = 1 };

    /**
     * Message from HLTSV to TTC2LAN.
     *
     * Change XON/XOFF status. Used to throttle the sender.
     * Payload: a single 32bit word, 0 = OFF, 1 == ON.
     */
    class XONOFF : public daq::asyncmsg::InputMessage {
    public:

        // This must agree with what the HLTSV uses
        static const uint32_t ID = 0x00DCDF50;
        
        XONOFF()
            : m_on_off(XONOFFStatus::ON)
        {}

        // Boiler plate for messages.
        
        // The message identifier
        uint32_t typeId() const override 
        {
            return ID;
        }

        // The transaction ID, not used.
        uint32_t transactionId() const override
        {
            return 0;
        }

        // Provide receive buffers.
        void toBuffers(std::vector<boost::asio::mutable_buffer>& buffers) override
        {
            buffers.push_back(boost::asio::buffer(&m_on_off, sizeof(m_on_off)));
        }

        // Message specific
        
        // Access received data, could also make variable just public in such
        // a simple case.
        XONOFFStatus status() const
        {
            return m_on_off;
        }

    private:
        XONOFFStatus m_on_off;
    };

    /**
     * Message from TTC2LAN to HLTSV.
     *
     * Event update.
     * Payload: a single extendend L1 ID (32bit)
     */
    class Update : public daq::asyncmsg::OutputMessage {
    public:

        // Must be same in HLTSV
        static const uint32_t ID = 0x00DCDF51;

        explicit Update(uint32_t l1id)
            : m_l1_id(l1id)
        {
        }

        // Message boiler plate
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
            buffers.push_back(boost::asio::buffer(&m_l1_id, sizeof(m_l1_id)));
        }

    private:

        uint32_t m_l1_id;

    };


    //
    // The Session class to talk to the HLTSV.
    //
    // The constructor takes a reference to a variable that it
    // updates with the XON/XOFF status as it receives messages.
    // This is the way it interacts with the rest of the application.
    //
    class Session : public daq::asyncmsg::Session {
    public:
        Session(boost::asio::io_service& service,
                std::atomic<XONOFFStatus>& status)
            : daq::asyncmsg::Session(service),
              m_status(status)
        {
        }

        // no copying please, just use the shared_ptr<Session>
        Session(const Session&) = delete;
        Session& operator=(const Session& ) = delete;
        
    protected:
        
        void onOpen() noexcept override 
        {
            ERS_LOG("Session is open:" << remoteEndpoint());

            // initiate first receive
            asyncReceive();
        }
        
        void onOpenError(const boost::system::error_code& error) noexcept override 
        {
            ERS_LOG("Open error..." << error);
        }
        
        void onClose() noexcept override
        {
            ERS_LOG("Session is closed");
        }
        
        void onCloseError(const boost::system::error_code& error) noexcept override
        {
            ERS_LOG("Close error..." << error);
        }
        
        //
        // This is called when the message header has been parsed. 
        // It gives us the chance to create an object specific for the message type.
        //
        // We only expect a XONOFF message, so that's all we check.
        //
        std::unique_ptr<daq::asyncmsg::InputMessage> 
        createMessage(std::uint32_t typeId,
                      std::uint32_t /* transactionId */, 
                      std::uint32_t size) noexcept override
        {
            ERS_ASSERT_MSG(typeId == XONOFF::ID, "Unexpected type ID in message: " << typeId);
            ERS_ASSERT_MSG(size == sizeof(uint32_t), "Unexpected size: " << size);
            ERS_DEBUG(3,"createMessage, size = " << size);

            // all ok, create a new XONOFF object
            return std::unique_ptr<XONOFF>(new XONOFF());
        }
        
        // 
        // This is called when the full message has been received.
        void onReceive(std::unique_ptr<daq::asyncmsg::InputMessage> message) override
        {
            ERS_DEBUG(3,"onReceive");
            std::unique_ptr<XONOFF> msg(dynamic_cast<XONOFF*>(message.release()));

            m_status = msg->status();
            
            ERS_LOG("Got XONOFF message, new status = " << (m_status == XONOFFStatus::ON ? "ON" : "OFF"));
            
            // Initiate next receive
            asyncReceive();
        }
        
        void onReceiveError(const boost::system::error_code& error,
                            std::unique_ptr<daq::asyncmsg::InputMessage> /* message */) noexcept override
        {
            ERS_LOG("Receive error: " << error);
        }
        
        void onSend(std::unique_ptr<const daq::asyncmsg::OutputMessage> /* message */) noexcept override
        {
            // Let the unique_ptr<> delete the message
        }
        
        void onSendError(const boost::system::error_code& error,
                         std::unique_ptr<const daq::asyncmsg::OutputMessage> /* message */) noexcept override
        {
            ERS_LOG("Send error: " << error);
            // throwing in the towel here, for this test program...
            abort();
        }

    private:
        std::atomic<XONOFFStatus>& m_status;
        std::atomic<bool>          m_running;
    };
    
    //
    // A simple run controlled application.
    //
    // It creates the session object, connects to the HLT supervisor
    // then runs a thread that generates L1 ID updates.
    // 
    class TTC2LANApplication : public daq::rc::Controllable {
    public:
        TTC2LANApplication()
            : daq::rc::Controllable(), m_running(false)
        {
            m_status = XONOFFStatus::ON;
          
          ERS_LOG("TTC2LANApplication(): m_status = "<< ((m_status == XONOFFStatus::ON) ? 1 : 0) );
        }

        ~TTC2LANApplication() noexcept
        {
        }

        virtual void disconnect(const daq::rc::TransitionCmd& ) override
        {
            m_session->asyncClose();

            // We should not return from this transition before the session is open;
            // this is rather clumsy but since we have only one connection...
            while(m_session->state() != daq::asyncmsg::Session::State::CLOSED) {
                usleep(10000);
            }

            m_work.reset();
            m_service.stop();
            m_io_thread.join();

        }
    
        virtual void connect(const daq::rc::TransitionCmd& ) override
        {
            // Get the partition from somewhere, assume it's in the environment for this test program.
          ERS_LOG("getting IPC partition: "<< getenv("TDAQ_PARTITION"));
            IPCPartition partition(getenv("TDAQ_PARTITION"));

            // For publishing our event counter status.
            m_dict.reset(new ISInfoDictionary(partition));

            // Create the name service object to lookup the HLTSV address
            daq::asyncmsg::NameService name_service(partition, std::vector<std::string>());

          
            // for debugging: find all applications and their endpoints
          std::map<std::string,boost::asio::ip::tcp::endpoint> apps;
          apps = name_service.lookup_names("");
          std::map<std::string,boost::asio::ip::tcp::endpoint>::iterator iter;
          ERS_LOG("applications found: ");
          for (iter = apps.begin(); iter != apps.end(); iter++) {
            ERS_LOG(iter->first);
          }
            // Do the lookup, the HLTSV will publish its endpoint under 'TTC2LANReceiver'.
            // In IS you can see it as the object 'DF.MSG_TTC2LANReceiver'.
            // This will throw an exception if it can't find the name entry.
            auto addr = name_service.resolve("TTC2LANReceiver");

            m_work.reset(new boost::asio::io_service::work(m_service));

            // Start the Boost.ASIO thread
            m_io_thread = std::thread([&]() { ERS_LOG("io_service starting"); m_service.run(); ERS_LOG("io_service finished"); });
            
            // Create the session with the endpoint we just got
            m_session = std::make_shared<Session>(m_service, m_status);
            m_session->asyncOpen(daq::rc::OnlineServices::instance().applicationName(), addr);

            m_event_count = 0;
            m_next_event = 0x01000000;

            // We should not return from this transition before the session is open;
            // this is rather clumsy but since we have only one connection...

            while(m_session->state() != daq::asyncmsg::Session::State::OPEN) {
                usleep(10000);
            }
          
          ERS_LOG("TTC2LANApplication()::connect(): m_status = "<< ((m_status == XONOFFStatus::ON) ? 1 : 0 ));
        }

        virtual void stopROIB(const daq::rc::TransitionCmd& ) override
        {
            // We pretend to be an RoIBuilder, so we should stop
            // in this transition.
            
            m_running = false;
            m_generator_thread.join();
        }

        virtual void prepareForRun(const daq::rc::TransitionCmd& ) override
        {
            m_running = true;
            m_generator_thread = std::thread(&TTC2LANApplication::generate_events, this);
            ERS_LOG("TTC2LANApplication()::prepareForRun(): m_status = "<< ((m_status == XONOFFStatus::ON) ? 1 : 0) );
        }

        virtual void publish() override
        {
            ERS_LOG("TTC2LANApplication()::publish(): m_status = "<< ((m_status == XONOFFStatus::ON) ? 1 : 0 ));
            // Here we publish the total event counter to IS.
            ISInfoUnsignedLong value;
            value.setValue(m_event_count);
            m_dict->checkin( "DF." + daq::rc::OnlineServices::instance().applicationName(), value);
        }

        // The event generator is here for this test program since it's easier
        // to directly access the xon/xoff status
        void generate_events()
        {
            ERS_LOG("TTC2LANApplication()::generateEvents(): m_status = "<< ((m_status == XONOFFStatus::ON) ? 1 : 0) );
            while(m_running) {

                // We can't really reliably sleep for less than a millisecond
                // since this is what the Linux scheduler gives us.
                std::this_thread::sleep_for(std::chrono::milliseconds(500));

                // If XON
                if(m_status == XONOFFStatus::ON) {

                    // Send an update
                    // Here we just increment the counter by 100, this gives
                    // us a 10 kHz event rate. We keep track of the overall
                    // number of events in a 64bit counter, so we can publish
                    // that to IS.

                    m_event_count += 100;
                    m_next_event  += 100;

                    // Create the message.
                    std::unique_ptr<Update> msg(new Update(m_next_event));

                    // Send the message. The std::move() transfers the ownership
                    // to the message passing library. The object is passed back
                    // to us via Session::onSend() when the message can be deleted.
                    m_session->asyncSend(std::move(msg));
                }
            }
        }

    private:
        std::thread                  m_io_thread;
        std::thread                  m_generator_thread;
        bool                         m_running;
        boost::asio::io_service      m_service;
        std::unique_ptr<boost::asio::io_service::work> m_work;
        std::shared_ptr<Session>     m_session;
        std::unique_ptr<ISInfoDictionary> m_dict;

        std::atomic<XONOFFStatus>    m_status;
        uint32_t                     m_next_event;
        uint64_t                     m_event_count;
    };

}

//
// Boilerplate main program
//
int main(int argc, char *argv[])
{
    try {
        daq::rc::CmdLineParser cmdline(argc, argv);
        daq::rc::ItemCtrl control(cmdline, std::make_shared<TTC2LANApplication>());
        control.init();
        control.run();
    } catch(ers::Issue& ex) {
        ers::fatal(ex);
        exit(EXIT_SUCCESS);
    }
}
