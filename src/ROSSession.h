// this is -*- C++ -*-
#ifndef HLTSV_ROSSESSION_H_
#define HLTSV_ROSSESSION_H_

#include <memory>
#include <list>

#include "asyncmsg/Session.h"

namespace hltsv {

    /**
     * \brief The connection to the ROS in case TCP is used for the clear messages.
     * 
     * Note that only the send operation is being used.
     */
    class ROSSession : public daq::asyncmsg::Session   {
    public:
        ROSSession(boost::asio::io_service& service);
        ~ROSSession();

        // Session interface
        virtual void onOpen() noexcept override;
        virtual void onOpenError(const boost::system::error_code& error) noexcept override;

        virtual void onClose() noexcept override;
        virtual void onCloseError(const boost::system::error_code& error) noexcept override;

        virtual std::unique_ptr<daq::asyncmsg::InputMessage> 
        createMessage(std::uint32_t typeId, std::uint32_t transactionId, std::uint32_t size) noexcept override;

        virtual void onReceive(std::unique_ptr<daq::asyncmsg::InputMessage> message) override;
        virtual void onReceiveError(const boost::system::error_code& error, std::unique_ptr<daq::asyncmsg::InputMessage> message) noexcept override;

        virtual void onSend(std::unique_ptr<const daq::asyncmsg::OutputMessage> message) noexcept override;
        virtual void onSendError(const boost::system::error_code& error, std::unique_ptr<const daq::asyncmsg::OutputMessage> message) noexcept override;
    };

}

#endif // HLTSV_ROSSESSION_H_
