
#include "MulticastROSClear.h"
#include "ClearMessage.h"
#include "ers/ers.h"

#include <boost/date_time/posix_time/posix_time.hpp>

namespace hltsv {

    class KeepAliveMessage : public daq::asyncmsg::OutputMessage {
    public:

        static const uint32_t ID = 0x00DCDF11;
        
         KeepAliveMessage() {};
        ~KeepAliveMessage() {};
        virtual uint32_t typeId() const override { return ID;};
        virtual uint32_t transactionId() const override {return 0;};
        virtual void     toBuffers(std::vector<boost::asio::const_buffer>&) const override {};
    };

    class MCSession : public daq::asyncmsg::UDPSession {
    public:
        MCSession(boost::asio::io_service& service)
            : daq::asyncmsg::UDPSession(service)
        {
        }

    protected:
        std::unique_ptr<daq::asyncmsg::InputMessage> createMessage(uint32_t, uint32_t, uint32_t) noexcept override
        {
            ERS_ASSERT_MSG(false, "Should never happen");
            return std::unique_ptr<daq::asyncmsg::InputMessage>();
        }


        void onReceive(std::unique_ptr<daq::asyncmsg::InputMessage>) noexcept override
        {
            ERS_ASSERT_MSG(false, "Should never happen");
        }

        void onReceiveError(const boost::system::error_code&, std::unique_ptr<daq::asyncmsg::InputMessage>) noexcept override
        {
            ERS_ASSERT_MSG(false, "Should never happen");
        }

        void onSend(std::unique_ptr<const daq::asyncmsg::OutputMessage>) noexcept override
        {
            // just delete message
        }

        void onSendError(const boost::system::error_code& error, std::unique_ptr<const daq::asyncmsg::OutputMessage>) noexcept override
        {
            ERS_LOG("send error: " << error);
        }
        
    };

    MulticastROSClear::MulticastROSClear(size_t threshold, boost::asio::io_service& service, const std::string& multicast_address, const std::string& outgoing)
        : ROSClear(threshold),
          m_session(new MCSession(service)),
	  m_timer(service)
    {
        m_session->setOutgoingInterface(daq::asyncmsg::NameService::find_interface(outgoing));
        m_session->connect(boost::asio::ip::udp::endpoint(boost::asio::ip::address::from_string(multicast_address), 9000));
    }

    MulticastROSClear::~MulticastROSClear()
    {
      //stop the keepalive
      m_timer.cancel();
    }

    void MulticastROSClear::connect()
    {
      // Give some time to the ROSs to join the group
      ::usleep(500000);

      // Start the keepalive
      m_timer.expires_from_now(boost::posix_time::seconds(0));
      this->do_ping(boost::system::error_code());
    }

    void MulticastROSClear::do_flush(uint32_t sequence, std::shared_ptr<std::vector<uint32_t>> events)
    {
        using namespace daq::asyncmsg;

        std::unique_ptr<const OutputMessage> msg(new ClearMessage(sequence,events));
        m_session->asyncSend(std::move(msg));
    }

    void MulticastROSClear::do_ping(const boost::system::error_code& e)
    {
      if (e != boost::asio::error::operation_aborted) {
	//send the keepalive
	std::unique_ptr<const daq::asyncmsg::OutputMessage> 
	  msg(new KeepAliveMessage());
	m_session->asyncSend(std::move(msg));
	
	m_timer.expires_at(m_timer.expires_at() + boost::posix_time::seconds(30));
	m_timer.async_wait(std::bind(&MulticastROSClear::do_ping, this, 
				    std::placeholders::_1));
      }
    }
}

