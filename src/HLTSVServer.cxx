
#include "HLTSVServer.h"
#include "DCMSession.h"

#include "ers/ers.h"
#include "monsvc/MonitoringService.h"

#include "TH1F.h"

#include <thread>

namespace hltsv {
    
    HLTSVServer::HLTSVServer(std::vector<boost::asio::io_service>& services,
                             std::shared_ptr<EventScheduler> scheduler,
                             std::shared_ptr<ROSClear> clear,
                             unsigned int timeout_in_ms,
                             monsvc::ptr<HLTSV> stats)
        : daq::asyncmsg::Server(services[0]),
          m_services(services),
          m_next_service(0),
          m_scheduler(scheduler),
          m_ros_clear(clear),
          m_timeout_in_ms(timeout_in_ms),
          m_time_histo(monsvc::MonitoringService::instance().register_object("ProcessingTime", 
                                                                             new TH1F("Event Processing Time", "Event Processing Time", 1000, 0., 1000.),
                                                                             true)),
          m_stats(stats)
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

        auto session = std::make_shared<DCMSession>(m_services[m_next_service],
                                                    m_scheduler,
                                                    m_ros_clear,
                                                    m_timeout_in_ms,
                                                    m_time_histo,
                                                    m_stats);

        m_next_service = (m_next_service + 1) % m_services.size();

        asyncAccept(session);
    }

    void HLTSVServer::stop()
    {
      using namespace daq::asyncmsg;

      close();

      // DCM should have closed the sessions already
      // clean up the rest.

      for(auto session : m_sessions) {
        if(session->state() != Session::State::CLOSED) {
          ERS_LOG("Session still open: " << session->remoteName());
          session->asyncClose();
        }
      }

      for(auto session : m_sessions) {

        if(session->state() != Session::State::CLOSED) {
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        if(session->state() != Session::State::CLOSED) {        
          ERS_LOG("Session still open after close and 10ms wait: " << session->remoteName());
        }
      }

      m_sessions.clear();
    }

    void HLTSVServer::onAccept(std::shared_ptr<daq::asyncmsg::Session> session) noexcept 
    {
        // save a reference to the new session
        std::shared_ptr<DCMSession> dcm(session, dynamic_cast<DCMSession*>(session.get()));

        m_sessions.push_back(dcm);
	
	auto hltsv = m_stats.get();
	hltsv->DCMSessions = m_sessions.size();
        
	// start the next accept
        auto new_session = std::make_shared<DCMSession>(m_services[m_next_service],
                                                        m_scheduler,
                                                        m_ros_clear,
                                                        m_timeout_in_ms,
                                                        m_time_histo,
                                                        m_stats);

        m_next_service = (m_next_service + 1) % m_services.size();

        asyncAccept(new_session);
    }

    void HLTSVServer::onAcceptError(const boost::system::error_code& error,
                                    std::shared_ptr<daq::asyncmsg::Session> ) noexcept
    {
      ERS_LOG("onAcceptError: " << error.message());
    }
}

