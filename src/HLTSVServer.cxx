
#include "HLTSVServer.h"
#include "DCMSession.h"

#include "ers/ers.h"
#include "monsvc/MonitoringService.h"

#include "TH1F.h"

namespace hltsv {
    
    HLTSVServer::HLTSVServer(boost::asio::io_service& service,
                             std::shared_ptr<EventScheduler> scheduler,
                             std::shared_ptr<ROSClear> clear,
                             unsigned int timeout_in_ms)
        : daq::asyncmsg::Server(service),
          m_service(service),
          m_scheduler(scheduler),
          m_ros_clear(clear),
          m_timeout_in_ms(timeout_in_ms),
          m_time_histo(monsvc::MonitoringService::instance().register_object("ProcessingTime", 
                                                                             new TH1F("Event Processing Time", "Event Processing Time", 1000, 0., 1000.),
                                                                             true))
    {
    }

    HLTSVServer::~HLTSVServer()
    {
        stop();
        monsvc::MonitoringService::instance().remove_object(std::string("ProcessingTime"));
    }

    void HLTSVServer::start()
    {
        m_time_histo->Reset();

        auto session = std::make_shared<DCMSession>(m_service,
                                                    m_scheduler,
                                                    m_ros_clear,
                                                    m_timeout_in_ms,
                                                    m_time_histo);
        asyncAccept(session);
    }

    void HLTSVServer::stop()
    {
        close();
        for(auto session : m_sessions) {
            session->close();
        }
        m_sessions.clear();
    }

    void HLTSVServer::onAccept(std::shared_ptr<daq::asyncmsg::Session> session) noexcept 
    {
	ERS_LOG("HLTSV Server accepted a DCM connection!");
        // save a reference to the new session
        std::shared_ptr<DCMSession> dcm(session, dynamic_cast<DCMSession*>(session.get()));

        m_sessions.push_back(dcm);
            
        // start the next accept
        auto new_session = std::make_shared<DCMSession>(m_service,
                                                        m_scheduler,
                                                        m_ros_clear,
                                                        m_timeout_in_ms,
                                                        m_time_histo);
        asyncAccept(new_session);
    }

    void HLTSVServer::onAcceptError(const boost::system::error_code& error,
                                    std::shared_ptr<daq::asyncmsg::Session> ) noexcept
    {
        ERS_LOG("onAcceptError: " << error);
    }
}

