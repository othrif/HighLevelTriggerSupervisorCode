
#include "LVL1Result.h"

namespace hltsv {


    LVL1Result::~LVL1Result()
    {
        for(auto ptr : m_data) m_deleter(ptr);
    }

    uint32_t LVL1Result::l1_id() const
    {
        return m_lvl1_id;
    }

    bool     LVL1Result::reassigned() const
    {
        return m_reassigned;
    }

    void     LVL1Result::set_reassigned()
    {
        m_reassigned = true;
    }

    LVL1Result::time_point LVL1Result::timestamp() const
    {
        return m_time_stamp;
    }

    void LVL1Result::set_timestamp(time_point ts)
    {
        m_time_stamp = ts;
    }

    uint64_t   LVL1Result::global_id() const
    {
        return m_global_id;
    }
    
    void LVL1Result::set_global_id(uint64_t global_id)
    {
        m_global_id = global_id;
    }

    size_t LVL1Result::event_data_size() const
    {
        size_t result = 0;
        for(auto len : m_lengths) {
            result += len * sizeof(uint32_t);
        }
        return result;
    }

    void LVL1Result::copy(uint32_t *target) const
    {
        for(size_t i = 0; i < m_data.size(); i++) {
            memcpy(target, m_data[i], m_lengths[i] * sizeof(uint32_t));
            target += m_lengths[i];
        }
    }


}

