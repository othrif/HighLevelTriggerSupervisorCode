// -*- c++ -*-
#ifndef HLTSV_L1TTCSOURCE_H_
#define HLTSV_L1TTCSOURCE_H_

/*
 * The L1TTCSource class encapsulates the version of the
 * L1Source class which provides a LVL1Result object 
 * from a buffer retunred by a TTCpr device
 *
 */

#include "L1Source.h"

#include "ttcpr/TTCpr.h"

namespace hltsv {

    class L1TTCSource : public L1Source {
    public:
        L1TTCSource();
        ~L1TTCSource();
        
        virtual dcmessages::LVL1Result* getResult();
        virtual void                    reset();
        
    private:
        TTCpr*        m_ttc;
        event_id*     m_result;
        int           m_internalECR;
        
    };
}

#endif // HLTSV_L1TTCSOURCE_H_

