
#include "src/DCMMessages.h"
#include "src/HLTSVSession.h"

namespace hltsv {

  HLTSVSession::HLTSVSession(boost::asio::io_service& service): daq::asyncmsg::Session(service)
  {
  }
  
  HLTSVSession::~HLTSVSession()
  {
  }
  
  
  void HLTSVSession::onOpen() noexcept
  {
  }
  
  void HLTSVSession::onOpenError(const boost::system::error_code& error) noexcept
  {
  }
  
  std::unique_ptr<daq::asyncmsg::InputMessage> 
  HLTSVSession::createMessage(std::uint32_t typeId, std::uint32_t transactionId, std::uint32_t size) noexcept
  {
    return std::unique_ptr<daq::asyncmsg::InputMessage>();
  }
  
  void HLTSVSession::onReceive(std::unique_ptr<daq::asyncmsg::InputMessage> message)
  {
  }
  
  void HLTSVSession::onReceiveError(const boost::system::error_code& error, std::unique_ptr<daq::asyncmsg::InputMessage> message) noexcept
  {
  }
  
  void HLTSVSession::onSend(std::unique_ptr<const daq::asyncmsg::OutputMessage> message) noexcept
  {
  }
  
  void HLTSVSession::onSendError(const boost::system::error_code& error, std::unique_ptr<const daq::asyncmsg::OutputMessage> message) noexcept
  {
  }
  
}
