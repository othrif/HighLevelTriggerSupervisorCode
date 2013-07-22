#ifndef HLTSV_L1INTERNALSOURCE_H_
#define HLTSV_L1INTERNALSOURCE_H_

#include "L1Source.h"

#include "DFdal/RoIBPluginInternal.h"

namespace hltsv {
    /**
     * \brief The L1InternalSource class encapsulates the version of the
     * L1Source class which provides a LVL1Result object internally
     * from configuration parameters.
     *
     */
    class L1InternalSource : public L1Source {
    public:
        L1InternalSource(const daq::df::RoIBPluginInternal *config);
        ~L1InternalSource();
        
        virtual LVL1Result* getResult() override;
        virtual void        reset(uint32_t /* run_number */ ) override;
        
    private:
        unsigned int          m_l1id;
        uint32_t              m_size;
        std::vector<uint32_t> m_dummy_data;
    };
}

#endif // HLTSV_L1INTERNALSOURCE_H_
