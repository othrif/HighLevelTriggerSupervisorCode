#include "ers/ers.h"

#include "DCMMessages.h"
#include "HLTSVSession.h"

namespace hltsv {

  HLTSVSession::HLTSVSession(boost::asio::io_service& service): daq::asyncmsg::Session(service)
  {
  }
  
  HLTSVSession::~HLTSVSession()
  {
  }
  
  
  void HLTSVSession::onOpen() noexcept
  {
    ERS_LOG(" *** CONNECTED TO HLTSV ***");
  }
  
  void HLTSVSession::onOpenError(const boost::system::error_code& error) noexcept
  {
    ERS_LOG(" *** Error opening connection to HLTSV! *** " << error);
  }
  
  std::unique_ptr<daq::asyncmsg::InputMessage> 
  HLTSVSession::createMessage(std::uint32_t typeId, std::uint32_t /*transactionId*/, std::uint32_t size) noexcept
  {
    switch (typeId)
    {
      case AssignMessage::ID:
        return std::unique_ptr<daq::asyncmsg::InputMessage>(new AssignMessage(size));
        break;
      case BuildMessage::ID:
        return std::unique_ptr<daq::asyncmsg::InputMessage>(new BuildMessage(size));
        break;
      default:
        break;
    }
    ERS_LOG(" *** HLTSVSession: Warning! Created unknown message type. *** ");
    return std::unique_ptr<daq::asyncmsg::InputMessage>(new AssignMessage(size));
  }
  
  void HLTSVSession::onReceive(std::unique_ptr<daq::asyncmsg::InputMessage> message)
  {
    bool build_only = false;
    switch (message->typeId())
    {
      default:
          ERS_LOG(" *** Warning: received unknown message ID: " << message->typeId() << " *** ");
          break;

      case BuildMessage::ID:
          // force build, fall through to AssignMessage case
          build_only = true;
          ERS_DEBUG(1, "Received force-accept assignment.");

      case AssignMessage::ID:
          std::unique_ptr<AssignMessage> msg(dynamic_cast<AssignMessage*>(message.release()));
          ERS_DEBUG(1,"got global ID: " << msg->global_id());
          // grab only the l1id and put it on the list of assigned/build IDs
          m_assigned_l1ids.push(std::make_tuple(build_only, msg->lvl1_id()));
          break;
    }

    // get ready to recieve another message
    asyncReceive();
  }
  
  void HLTSVSession::onReceiveError(const boost::system::error_code& error, std::unique_ptr<daq::asyncmsg::InputMessage> ) noexcept
  {
    ERS_LOG("HLTSVSession::onReceiveError with code: " << error);
  }
  
  void HLTSVSession::onSend(std::unique_ptr<const daq::asyncmsg::OutputMessage> ) noexcept
  {
  }
  
  void HLTSVSession::onSendError(const boost::system::error_code& error, std::unique_ptr<const daq::asyncmsg::OutputMessage> ) noexcept
  {
    ERS_LOG("sendError: " << error);
  }

  //******************************************

  void HLTSVSession::send_update(uint32_t req_RoIs, const std::vector<uint32_t> &l1ids)
  {
    std::unique_ptr<const hltsv::RequestMessage> update_msg(new hltsv::RequestMessage(req_RoIs, l1ids));
    asyncSend(std::move(update_msg));
  }

  std::tuple<bool,uint32_t> HLTSVSession::get_next_assignment()
  {
    std::tuple<bool,uint32_t> l1id;
    m_assigned_l1ids.pop(l1id);
    return l1id;
  }

  void HLTSVSession::abort_assignment_queue()
  {
    // wake up any threads waiting for a new assignment
    m_assigned_l1ids.abort();
  }

}

