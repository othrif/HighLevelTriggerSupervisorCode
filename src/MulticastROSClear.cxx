
#include "MulticastROSClear.h"

namespace hltsv {


    MulticastROSClear::MulticastROSClear(size_t threshold, boost::asio::io_service& service, const std::string& address_network)
        : ROSClear(threshold),
          m_socket(service)
    {
    }

    MulticastROSClear::~MulticastROSClear()
    {
    }

    void MulticastROSClear::connect()
    {
        // TODO:
        // 
        // - initialize the multicast socket with the given
        //   address and outgoing network interface.

    }

    void MulticastROSClear::do_flush(uint32_t sequence, std::shared_ptr<std::vector<uint32_t>> events)
    {

        // 
        // TODO
        //
        //   async_send(m_socket, ...);
        //   TODO: what about queuing ???
        //
        // 
    }

}

