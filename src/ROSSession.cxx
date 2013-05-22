
#include "ROSSession.h"

#include "ers/ers.h"

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
        ERS_LOG("opened connection to ROS: " << remoteName() << " addr: " << remoteEndpoint());
    }

    void ROSSession::onOpenError(const boost::system::error_code& error) noexcept  
    {
        ERS_LOG("openError: " << error );
        // report error
    }

    void ROSSession::onClose() noexcept
    {
        ERS_LOG("closed connection to ROS: " << remoteName() << " addr: " << remoteEndpoint());
    }

    void ROSSession::onCloseError(const boost::system::error_code& error) noexcept
    {
        ERS_LOG("closeError: " << error );
        // report error
    }

    std::unique_ptr<daq::asyncmsg::InputMessage> 
    ROSSession::createMessage(std::uint32_t , std::uint32_t , std::uint32_t ) noexcept  
    {
        ERS_ASSERT_MSG(false, "Should never happen");
        return std::unique_ptr<daq::asyncmsg::InputMessage>();
    }

    void ROSSession::onReceive(std::unique_ptr<daq::asyncmsg::InputMessage> )  
    {
        ERS_ASSERT_MSG(false, "Should never happen");
    }
     
    void ROSSession::onReceiveError(const boost::system::error_code& error, std::unique_ptr<daq::asyncmsg::InputMessage> ) noexcept  
    {
        ERS_ASSERT_MSG(false, "Should never happen: " << error);
    }

    void ROSSession::onSend(std::unique_ptr<const daq::asyncmsg::OutputMessage> ) noexcept  
    {
        // let message get deleted -> will decrease shared_ptr count
    }

    void ROSSession::onSendError(const boost::system::error_code& error, std::unique_ptr<const daq::asyncmsg::OutputMessage> ) noexcept  
    {
        ERS_LOG("sendError: " << error << " from " << remoteName());
        // report error
        // ?? close session ? how to handle this ?
    }

}

