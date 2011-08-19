// this is -*- c++ -*-
#ifndef HLTSV_L1LTPSOURCE_H_
#define HLTSV_L1LTPSOURCE_H_

/*
 * The L1LTPSource class encapsulates the version of the
 * L1Source class which provides a LVL1Result object 
 * from a message containing LVL2Decisions sent from the LTP
 *
 */

#include <utility>

#include "L1Source.h"

#include "queues/NamedQueue.h"

namespace hltsv {

    class L1LTPSource : public L1Source {
    public:
        L1LTPSource();
        ~L1LTPSource();
        
        virtual dcmessages::LVL1Result* getResult();
        virtual void                    reset();
        
    private:
        std::queue<LVL1ID>                      m_l1Queue;
        NamedQueue<std::pair<LVL1ID,LVL1ID> >*  m_inputQueue;
        
    };
}

#endif // HLTSV_L1LTPSOURCE_H_
