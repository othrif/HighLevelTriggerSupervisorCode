// this is -*- C++ -*-
#ifndef HLTSV_UNICASTROSCLEAR_H_
#define HLTSV_UNICASTROSCLEAR_H_

#include "ROSClear.h"

namespace hltsv {

    class ROSSession;

    /**
     * Unicast implementation of ROSClear, using TCP.
     */
    class UnicastROSClear : public ROSClear {
    public:
        UnicastROSClear(size_t threshold, boost::asio::io_service& service, daq::asyncmsg::NameService& name_service);
        ~UnicastROSClear();

        virtual void connect() override;

    private:

        virtual void do_flush(uint32_t sequence, std::shared_ptr<std::vector<uint32_t>> events) override;

        // TODO:
        //
        // using asyncmsg::NameService,
        // lookup all 'CLEAR_*' entries and connect to them. 
        // Add each session to m_sessions.
        //
        // do_flush(data):
        //        //
        //   for(each session) {
        //      uniq_ptr<ClearMessage> msg(new ClearMessage(sequence, data));
        //      session->send(msg);
    private:
        std::vector<boost::asio::ip::tcp::endpoint>  m_endpoints;
        std::vector<std::shared_ptr<ROSSession>>     m_sessions;
    };

    
}

#endif // HLTSV_UNICASTROSCLEAR_H_
