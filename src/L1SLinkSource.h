// -*- c++ -*-
#ifndef HLTSV_L1SLINKSOURCE_H_
#define HLTSV_L1SLINKSOURCE_H_


#include "L1Source.h"

class SSPDev;


namespace hltsv {

    /**
     * The L1SLinkSource class encapsulates the version of the
     * L1Source class which provides a LVL1Result object 
     * from a buffer retunred by an slink device
     *
     */
    
    class L1SLinkSource : public L1Source {
    public:
        L1SLinkSource();
        ~L1SLinkSource();
    
        virtual dcmessages::LVL1Result* getResult();
        virtual void                    reset();
        
    private:
        SSPDev*       m_ssp;
        unsigned int* m_result;
    };
}

#endif // HLTSV_L1SLINKSOURCE_H_
