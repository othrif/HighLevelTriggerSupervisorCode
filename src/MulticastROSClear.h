// this is -*- C++ -*-
#ifndef HLTSV_MULTICASTROSCLEAR_H_
#define HLTSV_MULTICASTROSCLEAR_H_

#include "ROSClear.h"
#include "asyncmsg/UDPSession.h"

namespace hltsv {

    // Forward declaration for multicast session (only defined internally).
    class MCSession;

    /**
     * \brief Multicast implementation of ROSClear.
     */
    class MulticastROSClear : public ROSClear {
    public:
        MulticastROSClear(size_t threshold, boost::asio::io_service& service, const std::string& multicast_address, const std::string& outgoing);
        ~MulticastROSClear();

      virtual void connect() override;

    private:

        /// The implementation of the flush operation.
        virtual void do_flush(uint32_t sequence, std::shared_ptr<std::vector<uint32_t>> events) override;

        /// Periodically sends a UDP ping
        void do_ping(const boost::system::error_code& e);

        /// The multicast session.
        std::shared_ptr<MCSession> m_session;
      
        /// Timer for UDP "keepalive" messages
        boost::asio::deadline_timer m_timer;
    };

}

#endif // HLTSV_MULTICASTROSCLEAR_H_
