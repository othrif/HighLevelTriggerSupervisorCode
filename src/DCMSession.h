// this is -*- C++ -*-
#ifndef HLTSV_DCMSESSION_H_
#define HLTSV_DCMSESSION_H_

#include <memory>
#include <list>

#include "asyncmsg/Session.h"
#include "monsvc/ptr.h"

#include "boost/asio/steady_timer.hpp"

class TH1F;

namespace hltsv {


    // The ROIs received from RoIBuilder
    class LVL1Result;

    // The ROSClear interface
    class ROSClear;

    // The scheduler interface.
    class EventScheduler;

    /**
     * \brief The Session object representing a single DCM. 
     *
     * It inherits from the message passing session object.
     * There is one such object per DCM and
     * it implements the base class' methods to react to incoming
     * messages and errors.
     *
     */

    class DCMSession : public daq::asyncmsg::Session 
    {
    public:
        DCMSession(boost::asio::io_service& service,
                   std::shared_ptr<EventScheduler> scheduler,
                   std::shared_ptr<ROSClear> clear,
                   unsigned int timeout_in_ms,
                   monsvc::ptr<TH1F> time_histo);

        ~DCMSession();

        /**
         * Called back from EventScheduler when an event is available.
         * If the session cannot handle the event (e.g. because it's in 
         * an error state, it should return false.
         */
        bool handle_event(std::shared_ptr<LVL1Result> rois);

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

    private:
        // Potentially restart the timer for this sessions if necessary.
        void restart_timer();

        // The actual check for timeouts in the list of handled events
        void check_timeouts(const boost::system::error_code& error);

        // The scheduler, necessary to re-assign events
        std::shared_ptr<EventScheduler>        m_scheduler;

        // The ROSClear interface to add processed events to.
        std::shared_ptr<ROSClear>              m_clear;

        // The list of currently handled events.
        std::list<std::shared_ptr<LVL1Result>> m_events;

        // Is this session in an error state ?
        bool                                   m_in_error;

        // The deadline timer for the next timeout.
        boost::asio::steady_timer              m_timer;

        // The timeout of an event in milliseconds
        unsigned int                           m_timeout_in_ms;

        // A shared reference to the 'ProcessingTime' histogram.
        monsvc::ptr<TH1F>                      m_time_histo;
    };

}

#endif // HLTSV_DCMSESSION_H_
