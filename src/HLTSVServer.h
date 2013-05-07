// -*- c++ -*-
#ifndef HLTSV_HLTSVSERVER_H_
#define HLTSV_HLTSVSERVER_H_

#include "asyncmsg/Server.h"
#include "monsvc/ptr.h"

#include <memory>
#include <vector>

class TH1F;

namespace hltsv {

    class EventScheduler;
    class ROSClear;
    class DCMSession;

    class HLTSVServer : public daq::asyncmsg::Server {
    public:
        HLTSVServer(boost::asio::io_service& service,
                    std::shared_ptr<EventScheduler> scheduler,
                    std::shared_ptr<ROSClear> clear,
                    unsigned int timeout_in_ms);
        ~HLTSVServer();

        void start();
        void stop();

        // asyncmsg::Server interface
        virtual void onAccept(std::shared_ptr<daq::asyncmsg::Session> session) noexcept override;

        virtual void onAcceptError(const boost::system::error_code& error,
                                   std::shared_ptr<daq::asyncmsg::Session> session) noexcept override;

    private:
        boost::asio::io_service&        m_service;
        std::shared_ptr<EventScheduler> m_scheduler;
        std::shared_ptr<ROSClear>       m_ros_clear;
        unsigned int                    m_timeout_in_ms;

        std::vector<std::shared_ptr<DCMSession>> m_sessions;

        monsvc::ptr<TH1F>               m_time_histo;
    };

}

#endif // HLTSV_HLTSVSERVER_H_
