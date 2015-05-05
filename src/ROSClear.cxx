
#include "ROSClear.h"
#include "asyncmsg/NameService.h"

namespace hltsv {

    ROSClear::ROSClear(size_t threshold)
        : m_threshold(threshold),
          m_sequence()
    {
        m_event_ids.reserve(m_threshold);
    }

    ROSClear::~ROSClear()
    {
        // ERS_ASSERT(m_event_ids.empty());

        // We cannot call flush() here since the
        // derived object no longer exists at this point.
    }
        
    // Add event to group of processed events. If threshold
    // reached, send ROS clear message to ROS.
    void ROSClear::add_event(uint32_t l1_event_id)
    {
        tbb::spin_mutex::scoped_lock lock(m_mutex);
        m_event_ids.push_back(l1_event_id);

        if(m_event_ids.size() >= m_threshold) {

            // create empty vector inside shared_ptr
            auto data = std::make_shared<std::vector<uint32_t>>();

            // swap event vector with local data
            m_event_ids.swap(*data);

            // reserve  in advance
            m_event_ids.reserve(m_threshold);

            uint32_t seq = m_sequence++;

            do_flush(seq, data);
        }
    }

    void ROSClear::flush()
    {
        auto data = std::make_shared<std::vector<uint32_t>>();

        tbb::spin_mutex::scoped_lock lock(m_mutex);
        m_event_ids.swap(*data);
        uint32_t seq = m_sequence++;

        do_flush(seq, data);
    }

    void ROSClear::connect()
    {
    }

    void ROSClear::prepareForRun()
    {
        tbb::spin_mutex::scoped_lock lock(m_mutex);
        m_sequence = 0;
    }

}
