
#include "L1InternalSource.h"

#include "eformat/eformat.h"
#include "eformat/write/eformat.h"

#include "LVL1Result.h"
#include "L1Source.h"
#include "Issues.h"

#include "config/Configuration.h"

namespace hltsv {
    /**
     * \brief The L1InternalSource class encapsulates the version of the
     * L1Source class which provides a LVL1Result object internally
     * from configuration parameters.
     *
     */
    L1InternalSource::L1InternalSource(const daq::df::RoIBPluginInternal *config)
        : m_l1id(0),
          m_size(config->get_FragmentSize()),
          m_dummy_data(m_size)
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

        eformat::helper::SourceIdentifier src(eformat::TDAQ_CTP, 1);

        eformat::write::ROBFragment rob(src.code(), run_no, m_l1id, bc_id,
                                        lvl1_type, event_type, 
                                        m_size, &m_dummy_data[0], 
                                        eformat::STATUS_FRONT);

        auto fragment = new uint32_t[rob.size_word()];
        eformat::write::copy(*rob.bind(), fragment, rob.size_word());

        LVL1Result* l1Result = new LVL1Result(m_l1id, fragment, rob.size_word());
        m_l1id += 1;

        return l1Result;
    }


    void
    L1InternalSource::reset(uint32_t /* run_number */)
    {
        // temporary hack to initialize L1ID 
        m_l1id = 0;
    }

}

