//
// $Id: L1LTPSource.cxx 42536 2007-11-14 19:20:56Z jls $
//

#include <unistd.h>
#include <stdlib.h>

#include "eformat/eformat.h"
#include "eformat/write/eformat.h"
#include "dcmessages/LVL1ID.h"
#include "dcmessages/LVL1Result.h"

#include "Issues.h"
#include "L1LTPSource.h"

extern "C" hltsv::L1Source* create_source(const std::string&, Configuration& )
{
    return new hltsv::L1LTPSource();
}

namespace hltsv {

    L1LTPSource::L1LTPSource()
        : m_inputQueue(NamedQueue<std::pair<LVL1ID,LVL1ID> >::find("LTPTrigger"))
    {
        if(m_inputQueue == 0) {
            throw InitialisationFailure(ERS_HERE, "Cannot find queue: LTPTrigger");
        }
    }

    L1LTPSource::~L1LTPSource()
    {
    }

    dcmessages::LVL1Result* L1LTPSource::getResult()
    {

        // check for L1IDs in queue
        if (m_l1Queue.empty()) {

            // try to get decision message
            std::pair<LVL1ID, LVL1ID> l1ids;
            
            if (m_inputQueue->try_get(l1ids)) {

                // get the list of L1IDs
                for (LVL1ID id=l1ids.first; id<=l1ids.second; id++) {
                    m_l1Queue.push(id);
                }

            } else {
                // L1ID queue empty, no messages pending
                return 0;
            }
        }

        //create the ROB fragment 
        const uint32_t run_no	   = 0x0;
        const uint32_t bc_id	   = 0x1;
        const uint32_t lvl1_type = 0xff;
        const uint32_t event_type = 1;

        const uint32_t l1id = m_l1Queue.front();
        m_l1Queue.pop();

        //ROB
        uint32_t dummy_data[32];
        eformat::helper::SourceIdentifier src(eformat::TDAQ_CTP, 1);

        eformat::write::ROBFragment rob(src.code(), run_no, l1id, bc_id,
                                        lvl1_type, event_type, 32, dummy_data, 
                                        eformat::STATUS_FRONT);

        eformat::write::ROBFragment* robs[1];
        robs[0] = &rob;
        dcmessages::LVL1Result* l1Result =
            new dcmessages::LVL1Result( robs, 1 );

        return l1Result;
    }

    void
    L1LTPSource::reset()
    {
        // empty l1 queue
        while (!m_l1Queue.empty())
            m_l1Queue.pop();
    }

}
