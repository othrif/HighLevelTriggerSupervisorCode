// this is -*- c++ -*-
#ifndef HLTSV_L1INTERNALSOURCE_H_
#define HLTSV_L1INTERNALSOURCE_H_

#include "L1Source.h"

namespace hltsv {

    /**
     * The L1InternalSource class encapsulates the version of the
     * L1Source class which provides a LVL1Result object internally
     * from configuration parameters.
     *
     */
    class L1InternalSource : public L1Source {
    public:
        L1InternalSource();
        ~L1InternalSource();
    
        virtual dcmessages::LVL1Result* getResult();
        virtual void                    reset();

    private:
        unsigned int   m_l1id;
    };

}

#endif // HLTSV_L1INTERNALSOURCE_H_
