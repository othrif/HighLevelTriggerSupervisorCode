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
  HLTSVSession::createMessage(std::uint32_t , std::uint32_t , std::uint32_t size) noexcept
  {
    return std::unique_ptr<daq::asyncmsg::InputMessage>(new AssignMessage(size));
  }
  
  void HLTSVSession::onReceive(std::unique_ptr<daq::asyncmsg::InputMessage> message)
  {
    std::unique_ptr<AssignMessage> msg(dynamic_cast<AssignMessage*>(message.release()));

    // grab only the l1id and put it on the list of assigned IDs
    m_assigned_l1ids.push(msg->lvl1_id());

    ERS_DEBUG(1,"got global ID: " << msg->global_id());

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

  uint32_t HLTSVSession::get_next_assignment()
  {
    uint32_t l1id;
    m_assigned_l1ids.pop(l1id);
    return l1id;
  }
  
  void HLTSVSession::abort_queue()
  {
    // wake up any threads waiting for a new assignment
    m_assigned_l1ids.abort();
  }
}

