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
    ERS_LOG(" *** Error opening connection to HLTSV! *** ");
  }
  
  std::unique_ptr<daq::asyncmsg::InputMessage> 
  HLTSVSession::createMessage(std::uint32_t typeId, std::uint32_t transactionId, std::uint32_t size) noexcept
  {
    return std::unique_ptr<daq::asyncmsg::InputMessage>(new AssignMessage(size));
  }
  
  void HLTSVSession::onReceive(std::unique_ptr<daq::asyncmsg::InputMessage> message)
  {
    ERS_LOG("HLTSVSession::onReceive message of type: " << message->typeId());
  }
  
  void HLTSVSession::onReceiveError(const boost::system::error_code& error, std::unique_ptr<daq::asyncmsg::InputMessage> message) noexcept
  {
    ERS_LOG("HLTSVSession::onReceiveError with code: " << error);
  }
  
  void HLTSVSession::onSend(std::unique_ptr<const daq::asyncmsg::OutputMessage> message) noexcept
  {

    ERS_LOG("HLTSVSession::onSend");

  }
  
  void HLTSVSession::onSendError(const boost::system::error_code& error, std::unique_ptr<const daq::asyncmsg::OutputMessage> message) noexcept
  {
  }
  
}
