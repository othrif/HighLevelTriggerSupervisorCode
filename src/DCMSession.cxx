
#include "DCMSession.h"
#include "Messages.h"
#include "EventScheduler.h"
#include "ROSClear.h"
#include "LVL1Result.h"

#include "ers/ers.h"
#include "TH1F.h"

#include <functional>

namespace hltsv {

    DCMSession::DCMSession(boost::asio::io_service& service,
                           std::shared_ptr<EventScheduler> scheduler,
                           std::shared_ptr<ROSClear> clear,
                           unsigned int timeout_in_ms,
                           monsvc::ptr<TH1F> time_histo)
        : daq::asyncmsg::Session(service),
          m_scheduler(scheduler),
          m_clear(clear),
          m_in_error(false),
          m_timer(service),
          m_timeout_in_ms(timeout_in_ms),
          m_time_histo(time_histo)
    {
    }


    DCMSession::~DCMSession()
    {
        // make sure there are no outstanding events ? how ?
        // re-schedule them ?
    }

    bool DCMSession::handle_event(std::shared_ptr<LVL1Result> rois)
    {
        if(!m_in_error) {
            
            getStrand().dispatch([rois,this]() {
                                     
                                     // Create a ProcessMessage which ontains the ROIs
                                     ERS_DEBUG(1,"DCMSession::handle_event: #" << rois->l1_id());
                                     
                                     if(!rois->reassigned()) {
                                         std::unique_ptr<const hltsv::ProcessMessage> roi_msg(new hltsv::ProcessMessage(rois));
                                         asyncSend(std::move(roi_msg));
                                     } else {
                                         std::unique_ptr<const hltsv::BuildMessage> roi_msg(new hltsv::BuildMessage(rois));
                                         asyncSend(std::move(roi_msg));
                                     }

                                     rois->set_timestamp();

                                     m_events.push_back(rois);
            
                                     // this might start the timer for the first time
                                     restart_timer();
                                 });
            
            return true;
            
        } else {
            return false;
        }
    }


    void DCMSession::check_timeouts(const boost::system::error_code& error)
    {
        // If a new timeout is scheduled on the timer, we get
        // an operation_aborted error.

        if(!error) {             // ok, timer has expired

            // get current time
            auto now = LVL1Result::clock::now();

            // check all events on local list
            for(auto it = m_events.begin(); it != m_events.end(); ) {
                // if timestamp > timeout value, reschedule them         
                auto event = *it;

                if(event->timestamp() + std::chrono::milliseconds(m_timeout_in_ms) <= now) {
                    // reschedule this event
                    // ERS_DEBUG(1,"Reassigning: " << event->l1_id());
                    ERS_LOG("Reassigning timeout: " << event->l1_id() << " from " << remoteName() << " timestamp: " << event->timestamp().time_since_epoch().count() << " now: " << now.time_since_epoch().count());
                    m_scheduler->reassign_event(event);
                    it = m_events.erase(it);
                    continue;
                } else {
                    break;
                }
            }

            restart_timer();

        } else {
            // ERS_LOG("Timer aborted or cancelled")
        }
    }

    //
    // Session interface
    //

    void DCMSession::onOpen() noexcept 
    {
      ERS_LOG("DCMSession::onOpen() from " << remoteName()); 
      asyncReceive();
    }

    void DCMSession::onOpenError(const boost::system::error_code& error) noexcept
    {
        ERS_LOG("openError: " << error << " from " << remoteName());
        // TODO: report;  Who closes me ?
        m_in_error = true;
    }

    std::unique_ptr<daq::asyncmsg::InputMessage> 
    DCMSession::createMessage(std::uint32_t typeId, std::uint32_t /* transactionId */, std::uint32_t size) noexcept
    {
        ERS_ASSERT_MSG(typeId == UpdateMessage::ID, "Unexpected message type: " << typeId << " instead of " << UpdateMessage::ID);
        return std::unique_ptr<daq::asyncmsg::InputMessage>(new UpdateMessage(size));
    }
    
    void DCMSession::onReceive(std::unique_ptr<daq::asyncmsg::InputMessage> message)
    {
      std::unique_ptr<UpdateMessage> msg(dynamic_cast<UpdateMessage*>(message.release()));

      auto now = LVL1Result::clock::now();

      for(uint32_t i = 0; i < msg->num_l1ids(); i++) {
          auto ev = std::find_if(m_events.begin(), m_events.end(), [&](std::shared_ptr<LVL1Result> event) -> bool { return event->l1_id() == msg->l1id(i); });
          if(ev == m_events.end()) {
              ERS_LOG("Error: invalid event id: " << msg->l1id(i));
          } else {

              auto processing_time = now - (*ev)->timestamp();
              m_time_histo->Fill(std::chrono::duration_cast<std::chrono::milliseconds>(processing_time).count());

              ERS_DEBUG(1,"Erasing event: " << msg->l1id(i));
              // remove from this session's list
              m_events.erase(ev);

              // allow HLTSV to clear the event
              m_clear->add_event(msg->l1id(i));
          }
      }

      // Pass to the scheduler the number of requested RoIs
      auto s_this(std::static_pointer_cast<DCMSession>(shared_from_this()));
      m_scheduler->request_events(s_this, msg->num_request(), msg->num_l1ids());

      restart_timer();
      
      // DCM session is ready for receiving new messages
      asyncReceive();
    }

    void DCMSession::onReceiveError(const boost::system::error_code& error, std::unique_ptr<daq::asyncmsg::InputMessage> ) noexcept
    {
      ERS_LOG("DCMSession::onReceiveError, with error: " << error << " from " << remoteName());

      // TODO: report, do we close this connection ?
      m_in_error = true;

      // reassign events
      for(auto event : m_events) {
          m_scheduler->reassign_event(event);
      }

      m_events.clear();

    }

    void DCMSession::onSend(std::unique_ptr<const daq::asyncmsg::OutputMessage> ) noexcept
    {
    }
    
    void DCMSession::onSendError(const boost::system::error_code& error, std::unique_ptr<const daq::asyncmsg::OutputMessage> ) noexcept
    {
        ERS_LOG("Send error: " << error << " from " << remoteName());
        m_in_error = true;

        // reassign events
        for(auto event : m_events) {
            m_scheduler->reassign_event(event);
        }
        
        m_events.clear();
    }

    void DCMSession::restart_timer()
    {
        if(!m_events.empty()) {
            if(m_timer_cache != m_events.front()) {
                m_timer_cache = m_events.front();

                auto expire_duration = (m_events.front()->timestamp() + std::chrono::milliseconds(m_timeout_in_ms)) - LVL1Result::clock::now();

                // m_timer.expires_from_now(boost::posix_time::milliseconds(std::chrono::duration_cast<std::chrono::milliseconds>(expire_duration).count()));
                m_timer.expires_from_now(expire_duration);
                m_timer.async_wait(getStrand().wrap(std::bind(&DCMSession::check_timeouts, this, std::placeholders::_1)));
            }
        }
    }
}

