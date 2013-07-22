#include "L1RateLimitedInternalSource.h"

#include <thread>

namespace hltsv {
    L1RateLimitedInternalSource::L1RateLimitedInternalSource(
            const daq::df::RoIBPluginRateLimitedInternal *config) :
        L1InternalSource(config),
        m_trigger_count(0),
        m_granularity(config->get_Granularity())
    {
        // set the initial event rate and correction frequency
        set_rate(config->get_TargetRate());
    }

    L1RateLimitedInternalSource::~L1RateLimitedInternalSource()
    {
    }

    /**
     * Pause if neccesary to achieve the target rate.
     * Then return L1InternalSource::getResult()
     */
    LVL1Result* L1RateLimitedInternalSource::getResult()
    {
        if (m_trigger_count == 0) {
            m_t_last = clock::now();
        }

        if (m_trigger_count % m_correction_rate == 0) {
            clock::time_point now = clock::now();

            clock::duration t_elapsed = now - m_t_last;
            auto t_delta = m_trigger_count * m_t_nominal - t_elapsed;

            if (t_delta > m_granularity) {
                std::this_thread::sleep_for(t_delta);
            }
            if (t_delta < -1*m_granularity) {
                // going too slow! nothing really to do then...
            }
            
            if (m_trigger_count > MAXIMUM_TRIGGER_COUNT) {
                m_trigger_count = 0;
            }
        }
        m_trigger_count++;

        return L1InternalSource::getResult();
    }

    void L1RateLimitedInternalSource::reset(uint32_t run_number)
    {
        L1InternalSource::reset(run_number);
        m_trigger_count = 0;
    }

    /**
     * Calculate the reciprocol of the given rate in kHz,
     * so we can use only multiplicative operations.
     * Also sets the correction frequency, so that the clock is
     * check only after O(granularity) time has passed.
     */
    void L1RateLimitedInternalSource::set_rate(float rate)
    {
        float ms_count = 1./rate;
        unsigned long us_count = ms_count * 1e3;
        unsigned long ns_count = ms_count * 1e6;
        m_t_nominal = nanoseconds(ns_count);

        // try to limit the clock checking to a rate of ~1/granularity;
        // sooner than that is just overhead. If < 1us, just use the granularity.
        m_correction_rate = m_granularity.count() / ( (us_count>0) ? us_count : 1 );
        if (m_correction_rate == 0) {
            // doesn't make sense to look every 0 events!
            m_correction_rate = 1;
        }
    }

}   /* namespace hltsv */

