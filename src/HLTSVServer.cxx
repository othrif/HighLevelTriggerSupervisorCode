
#include "HLTSVServer.h"
#include "DCMSession.h"

#include "ers/ers.h"


namespace hltsv {
    
    HLTSVServer::HLTSVServer(boost::asio::io_service& service,
                             std::shared_ptr<EventScheduler> scheduler,
                             std::shared_ptr<ROSClear> clear)
        : daq::asyncmsg::Server(service),
          m_service(service),
          m_scheduler(scheduler),
          m_ros_clear(clear)
    {
    }

    HLTSVServer::~HLTSVServer()
    {
    }

    void HLTSVServer::start()
    {
        auto session = std::make_shared<DCMSession>(m_service,
                                                    m_scheduler,
                                                    m_ros_clear);
        asyncAccept(session);
    }

    void HLTSVServer::stop()
    {
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
                                                        m_ros_clear);
	ERS_LOG("HLTSV waiting for more connections");
        asyncAccept(new_session);
    }

    void HLTSVServer::onAcceptError(const boost::system::error_code& error,
                                    std::shared_ptr<daq::asyncmsg::Session> session) noexcept
    {
        // issue an error message
        // just let the session be deleted
    }
        
}

