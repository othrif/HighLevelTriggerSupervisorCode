
#include "eformat/eformat.h"
#include "eformat/write/eformat.h"
#include "dcmessages/LVL1Result.h"

#include "L1InternalSource.h"

extern "C" hltsv::L1Source *create_source(const std::string&, Configuration& )
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

    dcmessages::LVL1Result* L1InternalSource::getResult()
    {
        //create the ROB fragment 
        const uint32_t run_no	   = 0x0;
        const uint32_t bc_id	   = 0x1;
        const uint32_t lvl1_type = 0xff;
  
        uint32_t event_type = 0x0; // params->getLumiBlock(); ?????

        //ROB
        uint32_t dummy_data[32];

        eformat::helper::SourceIdentifier src(eformat::TDAQ_CTP, 1);

        eformat::write::ROBFragment rob(src.code(), run_no, m_l1id, bc_id,
                                        lvl1_type, event_type, 32, dummy_data, 
                                        eformat::STATUS_FRONT);

        eformat::write::ROBFragment* robs[1];
        robs[0] = &rob;
        dcmessages::LVL1Result* l1Result = new dcmessages::LVL1Result( robs, 1 );
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
