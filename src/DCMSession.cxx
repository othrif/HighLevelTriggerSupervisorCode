
#include "DCMSession.h"
#include "Messages.h"
#include "EventScheduler.h"

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
        //
        // call send(); let the onSend() method add the message
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
      
      // Put every LVL1 ID from the UpdateMessage to m_clear->add_event();
      ERS_LOG("msg=" << msg->num_request());
      
      // Pass to the scheduler the number of requested RoIs
      std::shared_ptr<DCMSession>test(this);
      m_scheduler->request_events(test, msg->num_request());
      
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
        // TODO: add to local list of sent events 
    }

    void DCMSession::onSendError(const boost::system::error_code& error, std::unique_ptr<const daq::asyncmsg::OutputMessage> message) noexcept
    {
        // TODO: recover the LVL1 info and re-schedule on another node.
    }

}

