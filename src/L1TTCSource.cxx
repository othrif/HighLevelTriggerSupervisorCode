
#include "eformat/eformat.h"
#include "eformat/write/eformat.h"
#include "dcmessages/LVL1Result.h"

#include "L1TTCSource.h"

#include "ttcpr/TTCpr.h"

extern "C" hltsv::L1Source *create_source(const std::string&, Configuration& )
{
    return new hltsv::L1TTCSource();
}

namespace hltsv {


    L1TTCSource::L1TTCSource() :
        m_ttc(0),
        m_result(0),
        m_internalECR(-1)
    {
        m_ttc = new TTCpr;

    }

    L1TTCSource::~L1TTCSource()
    {
        // Close TTC device
        delete m_ttc;
    }

    dcmessages::LVL1Result* L1TTCSource::getResult()
    {
        m_result = m_ttc->ids();
        if(m_result == 0) return 0;

        // increment ECR
        if (m_result->l1id == 0)
            m_internalECR++;

        //create the ROB fragment 
        const uint32_t run_no	   = 0x0;
        const uint32_t l1_id	   = m_result->l1id | (m_internalECR)<<24;;
        const uint32_t bc_id	   = m_result->bcid;
        const uint32_t lvl1_type = m_result->ttype;
        const uint32_t event_type = 1;

        //ROB
        uint32_t dummy_data[16];
        eformat::helper::SourceIdentifier src(eformat::TDAQ_CTP, 0);

        eformat::write::ROBFragment rob(src.code(), run_no, l1_id, bc_id,
                                        lvl1_type, event_type, 16, dummy_data, 
                                        eformat::STATUS_FRONT);

        eformat::write::ROBFragment* robs[1];
        robs[0] = &rob;
        dcmessages::LVL1Result* l1Result =
            new dcmessages::LVL1Result( robs, 1 );

        // initialize next io
        m_ttc->request_ids();

        return l1Result;
    }

    void
    L1TTCSource::reset()
    {
        // reset device
        m_ttc->reset();

        // set up
        m_ttc->clear();
        m_ttc->mode(1);
        m_ttc->event_threshold(1);
        m_internalECR = -1;

        // Initiate I/O
        m_ttc->request_ids();

    }
}
