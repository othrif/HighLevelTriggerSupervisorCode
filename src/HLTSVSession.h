// this is -*- C++ -*-
#ifndef HLTSVSESSION_H_
#define HLTSVSESSION_H_

#include <memory>
#include <list>

#include "asyncmsg/Session.h"

namespace hltsv {

  class HLTSVSession : public daq::asyncmsg::Session 
  {
  public:
    HLTSVSession(boost::asio::io_service& service);  
    ~HLTSVSession();
    
    // Session interface
    virtual void onOpen() noexcept override;
    virtual void onOpenError(const boost::system::error_code& error) noexcept override;
    virtual std::unique_ptr<daq::asyncmsg::InputMessage> createMessage(std::uint32_t typeId, std::uint32_t transactionId, std::uint32_t size) noexcept override;
    
    virtual void onReceive(std::unique_ptr<daq::asyncmsg::InputMessage> message) override;
    virtual void onReceiveError(const boost::system::error_code& error, std::unique_ptr<daq::asyncmsg::InputMessage> message) noexcept override;
    virtual void onSend(std::unique_ptr<const daq::asyncmsg::OutputMessage> message) noexcept override;
    virtual void onSendError(const boost::system::error_code& error, std::unique_ptr<const daq::asyncmsg::OutputMessage> message) noexcept override;
    
    
  };
  
}


#endif // HLTSVSESSION_H_
