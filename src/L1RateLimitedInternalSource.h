#ifndef HLTSV_L1RATELIMITEDINTERNALSOURCE_H_
#define HLTSV_L1RATELIMITEDINTERNALSOURCE_H_

#include "L1InternalSource.h"
#include "DFdal/RoIBPluginRateLimitedInternal.h"

#include <chrono>

namespace hltsv {
    /**
     * \breif The L1RateLimitedInternalSource is a simple extension to
     * the L1InternalSource which enforces an upper limit
     * to the L1 triggering rate.
     *
     * This works by solving:
     *    R = N / (t + delta)   =>   delta = N/R - t  
     * where R is the target rate, N is the number of events processed,
     * and t is the time to process N events.
     * delta is the amount of time needed to sleep to acheive the rate R.
     */
    class L1RateLimitedInternalSource : public L1InternalSource {
    public:
        typedef std::chrono::steady_clock clock;
        typedef std::chrono::microseconds microseconds;
        typedef std::chrono::nanoseconds nanoseconds;

        // Maximum number of events to average the rate over.
        // After this many events, the reference time is reset.
        // NOTE: this may cause a periodic jump in HLTSV rate! (but I haven't seen this yet)
        static const uint32_t MAXIMUM_TRIGGER_COUNT = 1e9;

        L1RateLimitedInternalSource(const daq::df::RoIBPluginRateLimitedInternal *config);
        ~L1RateLimitedInternalSource();

        virtual LVL1Result* getResult() override;
        virtual void reset(uint32_t run_number);

        void set_rate(float rate);

    private:
        uint32_t m_trigger_count;   // # of events processed since m_t_last
        uint32_t m_correction_rate; // # of events to skip between clock-checking
        nanoseconds m_t_nominal;    // # nanoseconds per evt at nominal rate
        microseconds m_granularity; // minimum duration required before sleeping
        clock::time_point m_t_last; // time since we started couting m_trigger_count
    };

}   /* namespace hltsv */

#endif // HLTSV_L1RATELIMITEDINTERNALSOURCE_H_

