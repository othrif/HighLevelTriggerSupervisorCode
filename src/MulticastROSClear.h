// this is -*- C++ -*-
#ifndef HLTSV_MULTICASTROSCLEAR_H_
#define HLTSV_MULTICASTROSCLEAR_H_

#include "ROSClear.h"

namespace hltsv {

    /**
     * Multicast implementation of ROSClear.
     */
    class MulticastROSClear : public ROSClear {
    public:
        MulticastROSClear(size_t threshold, boost::asio::io_service& service, const std::string& address_network);
        ~MulticastROSClear();

        void connect() override;

    private:

        virtual void do_flush(uint32_t sequence, std::shared_ptr<std::vector<uint32_t>> events) override;

        boost::asio::ip::udp::socket m_socket;
    };

}

#endif // HLTSV_MULTICASTROSCLEAR_H_
