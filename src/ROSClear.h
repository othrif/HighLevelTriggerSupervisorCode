// this is -*- C++ -*-
#ifndef HLTSV_ROSCLEAR_H_
#define HLTSV_ROSCLEAR_H_

#include <cstdint>
#include <mutex>
#include <vector>
#include <string>
#include <memory>

#include "boost/asio.hpp"
#include "ROSSession.h"
#include "asyncmsg/NameService.h"

namespace hltsv {

    /**
     * The ROSClear interface.
     * This is all the rest of the HLTSV sees.
     *
     * The connect() method should be called at the
     * appropriate time to setup the connections.
     */
    class ROSClear {
    public:
        
        /**
         * The threshold is the number of events after which
         * the internal list of event IDs is sent to the ROS.
         */
        explicit ROSClear(size_t threshold = 100);
        virtual ~ROSClear();

        ROSClear(const ROSClear& ) = delete;
        ROSClear& operator=(const ROSClear& ) = delete;

        /** 
         * Add event to group of processed events. If threshold
         * reached, send ROS clear message to ROS.
         */
        void add_event(uint32_t l1_event_id);

        /// Flush all existing events to the ROS.
        void flush();

        /// Do any connect operation that is necessary
        virtual void connect();

    protected:
        
        /** 
         * Send any existing events to the ROS.
         * This is implemented in a derived class, using either
         * multicast or multiple sends.
         */
        virtual void do_flush(uint32_t sequence, std::shared_ptr<std::vector<uint32_t>> events) = 0;

    private:
        size_t                m_threshold;
        uint32_t              m_sequence;
        std::mutex            m_mutex;
        std::vector<uint32_t> m_event_ids;
    };

}

#endif // HLTSV_ROSCLEAR_H_
