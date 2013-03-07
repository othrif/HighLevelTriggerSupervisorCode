
#include "ROSSession.h"
#include "Messages.h"

namespace hltsv {


    ROSSession::ROSSession(boost::asio::io_service& service)
        : daq::asyncmsg::Session(service)
    {
    }

    ROSSession::~ROSSession()
    {
    }

    // Session interface
    void ROSSession::onOpen() noexcept  
    {
    }

    void ROSSession::onOpenError(const boost::system::error_code& error) noexcept  
    {
    }
    
    std::unique_ptr<daq::asyncmsg::InputMessage> 
    ROSSession::createMessage(std::uint32_t typeId, std::uint32_t transactionId, std::uint32_t size) noexcept  
    {
        // ERS_ASSERT(false, "should never happen");
        return std::unique_ptr<daq::asyncmsg::InputMessage>();
    }

    void ROSSession::onReceive(std::unique_ptr<daq::asyncmsg::InputMessage> message)  
    {
    }
     
    void ROSSession::onReceiveError(const boost::system::error_code& error, std::unique_ptr<daq::asyncmsg::InputMessage> message) noexcept  
    {
    }

    void ROSSession::onSend(std::unique_ptr<const daq::asyncmsg::OutputMessage> message) noexcept  
    {
        // let message get deleted -> will decrease shared_ptr coun
    }

    void ROSSession::onSendError(const boost::system::error_code& error, std::unique_ptr<const daq::asyncmsg::OutputMessage> message) noexcept  
    {
        // ?? close session ? how to handle this ?
    }

}

