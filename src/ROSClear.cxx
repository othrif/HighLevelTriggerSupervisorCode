
#include "ROSClear.h"
#include "Messages.h"
#include "asyncmsg/NameService.h"

namespace hltsv {

    ROSClear::ROSClear(size_t threshold)
        : m_threshold(threshold)
    {
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
        std::unique_lock<std::mutex> lock(m_mutex);
        m_event_ids.push_back(l1_event_id);

        if(m_event_ids.size() >= m_threshold) {

            auto data = std::make_shared<std::vector<uint32_t>>();

            m_event_ids.swap(*data);
            uint32_t seq = m_sequence++;

            do_flush(seq, data);
        }
    }

    void ROSClear::flush()
    {
        auto data = std::make_shared<std::vector<uint32_t>>();

        std::unique_lock<std::mutex> lock(m_mutex);
        m_event_ids.swap(*data);
        uint32_t seq = m_sequence++;

        do_flush(seq, data);
    }

    void ROSClear::connect()
    {
    }

}
