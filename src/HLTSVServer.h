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

    /**
     * \brief The asyncmsg server object dealing with DCMs.
     *
     * All arguments are forwarded to the DCMSessions that are
     * created by this server.
     */

    class HLTSVServer : public daq::asyncmsg::Server {
    public:
        HLTSVServer(boost::asio::io_service& service,
                    std::shared_ptr<EventScheduler> scheduler,
                    std::shared_ptr<ROSClear> clear,
                    unsigned int timeout_in_ms);
        ~HLTSVServer();

        // Start and stop the server, i.e. accepting connections.
        void start();
        void stop();

        // asyncmsg::Server interface
        virtual void onAccept(std::shared_ptr<daq::asyncmsg::Session> session) noexcept override;
        virtual void onAcceptError(const boost::system::error_code& error,
                                   std::shared_ptr<daq::asyncmsg::Session> session) noexcept override;

    private:
        // Values that are forwarded to all DCMSessions.
        boost::asio::io_service&        m_service;
        std::shared_ptr<EventScheduler> m_scheduler;
        std::shared_ptr<ROSClear>       m_ros_clear;
        unsigned int                    m_timeout_in_ms;

        // A list of all DCMSession. 
        std::vector<std::shared_ptr<DCMSession>> m_sessions;

        // A reference to the ProcessingTime histogram. 
        // It is create internally and passed to all DCMSessions.
        monsvc::ptr<TH1F>               m_time_histo;
    };

}

#endif // HLTSV_HLTSVSERVER_H_
