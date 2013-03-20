// this is -*- c++ -*-
#ifndef HLTSV_L1FILARSOURCE_H_
#define HLTSV_L1FILARSOURCE_H_

#include "L1Source.h"

class CMemory;
class RingBuffer;

namespace hltsv {

    const int FILAR_POOL_SIZE = 0x10000;
    const int FILAR_BUFFER_COUNT = 0x10;

    /**
     * The L1FilarSource class encapsulates the version of the
     * L1Source class which provides a LVL1Result object 
     * from a buffer retunred by an filar device
     *
     */

    class L1FilarSource : public L1Source {
    public:
        L1FilarSource(const std::string& source_typ);
        ~L1FilarSource();
    
        virtual LVL1Result* getResult();
        virtual void        reset();
        virtual void        preset();

    private:

        bool resultAvailable();

        CMemory*      m_cmem;
        RingBuffer*   m_rb;

        int           m_size;
        unsigned int* m_result;
        std::string   m_nodeid;
        unsigned int  m_chan;

    };
}
#endif // HLTSV_L1FILARSOURCE_H_
