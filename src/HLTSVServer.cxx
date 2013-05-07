
#include "HLTSVServer.h"
#include "DCMSession.h"

#include "ers/ers.h"


namespace hltsv {
    
    HLTSVServer::HLTSVServer(boost::asio::io_service& service,
                             std::shared_ptr<EventScheduler> scheduler,
                             std::shared_ptr<ROSClear> clear,
                             unsigned int timeout_in_ms)
        : daq::asyncmsg::Server(service),
          m_service(service),
          m_scheduler(scheduler),
          m_ros_clear(clear),
          m_timeout_in_ms(timeout_in_ms)
    {
    }

    HLTSVServer::~HLTSVServer()
    {
        stop();
    }

    void HLTSVServer::start()
    {
        auto session = std::make_shared<DCMSession>(m_service,
                                                    m_scheduler,
                                                    m_ros_clear,
                                                    m_timeout_in_ms);
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
                                                        m_timeout_in_ms);
        asyncAccept(new_session);
    }

    void HLTSVServer::onAcceptError(const boost::system::error_code& error,
                                    std::shared_ptr<daq::asyncmsg::Session> ) noexcept
    {
        ERS_LOG("onAcceptError: " << error);
    }
        
}

