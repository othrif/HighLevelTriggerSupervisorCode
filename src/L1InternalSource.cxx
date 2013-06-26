
#include "eformat/eformat.h"
#include "eformat/write/eformat.h"

#include "L1InternalSource.h"
#include "LVL1Result.h"

extern "C" hltsv::L1Source *create_source(const std::string&, const std::vector<std::string>& /* unused */)
{
    return new hltsv::L1InternalSource();
}

namespace hltsv {

    L1InternalSource::L1InternalSource()
        : m_l1id(0)
    {
    }

    L1InternalSource::~L1InternalSource()
    {
    }

    LVL1Result* L1InternalSource::getResult()
    {
        //create the ROB fragment 
        const uint32_t run_no	   = 0x0;
        const uint32_t bc_id	   = 0x1;
        const uint32_t lvl1_type = 0xff;
  
        const uint32_t event_type = 0x0; // params->getLumiBlock(); ?????

        static const size_t dummy_size = getenv("TDAQ_HLTSV_CTPSIZE") ? strtoul(getenv("TDAQ_HLTSV_CTPSIZE"),0,0) : 250;
        static const uint32_t *dummy_data = new uint32_t[dummy_size];

        eformat::helper::SourceIdentifier src(eformat::TDAQ_CTP, 1);

        eformat::write::ROBFragment rob(src.code(), run_no, m_l1id, bc_id,
                                        lvl1_type, event_type, 
                                        dummy_size, dummy_data, 
                                        eformat::STATUS_FRONT);

        auto fragment = new uint32_t[rob.size_word()];
        eformat::write::copy(*rob.bind(), fragment, rob.size_word());

        LVL1Result* l1Result = new LVL1Result(m_l1id, fragment, rob.size_word());
        m_l1id += 1;

        return l1Result;
    }


    void
    L1InternalSource::reset()
    {
        // temporary hack to initialize L1ID 
        m_l1id = 0;
    }

}
