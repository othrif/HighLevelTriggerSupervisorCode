
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


}

