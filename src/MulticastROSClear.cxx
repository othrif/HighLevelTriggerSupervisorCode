
#include "MulticastROSClear.h"
#include "Messages.h"
#include "ers/ers.h"

namespace hltsv {

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
          m_session(new MCSession(service))
    {
        m_session->setOutgoingInterface(daq::asyncmsg::NameService::find_interface(outgoing));
        m_session->connect(boost::asio::ip::udp::endpoint(boost::asio::ip::address::from_string(multicast_address), 9000));
    }

    MulticastROSClear::~MulticastROSClear()
    {
    }

    void MulticastROSClear::do_flush(uint32_t sequence, std::shared_ptr<std::vector<uint32_t>> events)
    {
        using namespace daq::asyncmsg;

        std::unique_ptr<const OutputMessage> msg(new ClearMessage(sequence,events));
        m_session->asyncSend(std::move(msg));
    }

}

