// this is -*- C++ -*-
#ifndef HLTSV_UNICASTROSCLEAR_H_
#define HLTSV_UNICASTROSCLEAR_H_

#include "ROSClear.h"

namespace hltsv {

    class ROSSession;

    /**
     * \brief Unicast implementation of ROSClear, using TCP.
     */
    class UnicastROSClear : public ROSClear {
    public:
        UnicastROSClear(size_t threshold, boost::asio::io_service& service, daq::asyncmsg::NameService& name_service);
        ~UnicastROSClear();

        virtual void connect() override;

    private:
        /// The implementation of the flush operation.

        virtual void do_flush(uint32_t sequence, std::shared_ptr<std::vector<uint32_t>> events) override;
    private:
        boost::asio::io_service&                     m_service;
        daq::asyncmsg::NameService                   m_name_service;
        std::vector<std::shared_ptr<ROSSession>>     m_sessions;
    };

    
}

#endif // HLTSV_UNICASTROSCLEAR_H_
