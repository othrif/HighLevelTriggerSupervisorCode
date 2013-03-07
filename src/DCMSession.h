// this is -*- C++ -*-
#ifndef HLTSV_DCMSESSION_H_
#define HLTSV_DCMSESSION_H_

#include <memory>
#include <list>

#include "asyncmsg/Session.h"

namespace hltsv {

    // The ROIs received from RoIBuilder
    class LVL1Result;

    // The ROSClear interface
    class ROSClear;

    // The scheduler interface.
    class EventScheduler;

    /**
     * For now this is a place holder class. 
     *
     * It will inherit from the message passing session object.
     * The assumption is that there is one such object per DCM
     * and implement the base class' methods to react to incoming
     * messages and errors.
     *
     * The additional interface is the one that the scheduler needs.
     * 
     */

    class DCMSession : public daq::asyncmsg::Session 
    {
    public:
        DCMSession(boost::asio::io_service& service,
                   std::shared_ptr<EventScheduler> scheduler,
                   std::shared_ptr<ROSClear> clear);

        ~DCMSession();

        // Called back from EventScheduler when an event is available.
        void handle_event(std::shared_ptr<LVL1Result> rois);

        // Called by somebody...
        void check_timeouts();

        // Session interface

        virtual void onOpen() noexcept override;
        virtual void onOpenError(const boost::system::error_code& error) noexcept override;

        virtual std::unique_ptr<daq::asyncmsg::InputMessage> 
        createMessage(std::uint32_t typeId, std::uint32_t transactionId, std::uint32_t size) noexcept override;

        virtual void onReceive(std::unique_ptr<daq::asyncmsg::InputMessage> message) override;
        virtual void onReceiveError(const boost::system::error_code& error, std::unique_ptr<daq::asyncmsg::InputMessage> message) noexcept override;
        virtual void onSend(std::unique_ptr<const daq::asyncmsg::OutputMessage> message) noexcept override;
        virtual void onSendError(const boost::system::error_code& error, std::unique_ptr<const daq::asyncmsg::OutputMessage> message) noexcept override;

    private:
        std::shared_ptr<EventScheduler>        m_scheduler;
        std::shared_ptr<ROSClear>              m_clear;
        std::list<std::shared_ptr<LVL1Result>> m_events;
    };

}

#endif // HLTSV_DCMSESSION_H_
