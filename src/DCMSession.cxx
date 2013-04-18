
#include "DCMSession.h"
#include "Messages.h"
#include "EventScheduler.h"
#include "ROSClear.h"

#include "ers/ers.h"

namespace hltsv {

    DCMSession::DCMSession(boost::asio::io_service& service,
                           std::shared_ptr<EventScheduler> scheduler,
                           std::shared_ptr<ROSClear> clear)
        : daq::asyncmsg::Session(service),
          m_scheduler(scheduler),
          m_clear(clear)
    {
    }


    DCMSession::~DCMSession()
    {
        // make sure there are no outstanding events ? how ?
        // re-schedule them ?
    }

    void DCMSession::handle_event(std::shared_ptr<LVL1Result> rois)
    {
      // Create a ProcessMessage which ontains the ROIs
      ERS_LOG("DCMSession::handle_event");
      std::unique_ptr<const hltsv::ProcessMessage> roi_msg(new hltsv::ProcessMessage(rois));
      // call asyncSend();
      asyncSend(std::move(roi_msg));
        // let the onSend() method add the message
        // to the local list.
    }

    void DCMSession::check_timeouts()
    {
        // check all events on local list
        // if timestamp > timeout value, reschedule them
    }

    //
    // Session interface
    //

    void DCMSession::onOpen() noexcept 
    {
        // TODO: nothing really, we wait for the first UpdateMessage
      ERS_LOG("DCMSession::onOpen()");
      // each dcm session is ready to receive incoming messages
      asyncReceive();

    }

    void DCMSession::onOpenError(const boost::system::error_code& error) noexcept
    {
        // TODO: report ? who closes me ?
    }

    std::unique_ptr<daq::asyncmsg::InputMessage> 
    DCMSession::createMessage(std::uint32_t typeId, std::uint32_t transactionId, std::uint32_t size) noexcept
    {
        return std::unique_ptr<daq::asyncmsg::InputMessage>(new UpdateMessage(size));
    }
    
    void DCMSession::onReceive(std::unique_ptr<daq::asyncmsg::InputMessage> message)
    {
      // ERS_ASSERT(message->typeId() == UpdateMessage::ID)
      ERS_LOG("DCMSession::onReceive");
      std::unique_ptr<UpdateMessage> msg(dynamic_cast<UpdateMessage*>(message.release()));

      for(uint32_t i = 0; i < msg->num_l1ids(); i++) {
          m_clear->add_event(msg->l1id(i));
      }
      // Put every LVL1 ID from the UpdateMessage to m_clear->add_event();

      ERS_LOG("msg=" << msg->num_request());
      
      // Pass to the scheduler the number of requested RoIs
      auto s_this(std::static_pointer_cast<DCMSession>(shared_from_this()));
      m_scheduler->request_events(s_this, msg->num_request());
      
      ERS_LOG("DCMSession::onReceive done with request_event");
      // DCM session is ready for receiving new messages
      asyncReceive();
    }

    void DCMSession::onReceiveError(const boost::system::error_code& error, std::unique_ptr<daq::asyncmsg::InputMessage> message) noexcept
    {
      ERS_LOG("DCMSession::onReceiveError, with error: " << error);
	// TODO: report, do we close this connection ?
    }

    void DCMSession::onSend(std::unique_ptr<const daq::asyncmsg::OutputMessage> message) noexcept
    {
      ERS_LOG(" DCMSession::onSend");
        // TODO: add to local list of sent events 
    }

    void DCMSession::onSendError(const boost::system::error_code& error, std::unique_ptr<const daq::asyncmsg::OutputMessage> message) noexcept
    {
        // TODO: recover the LVL1 info and re-schedule on another node.
    }

}

