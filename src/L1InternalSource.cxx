
#include "eformat/eformat.h"
#include "eformat/write/eformat.h"

#include "LVL1Result.h"
#include "L1Source.h"
#include "Issues.h"

#include "config/Configuration.h"
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
        virtual void        reset(uint32_t run_number) override;
        
    private:
        unsigned int          m_l1id;
        uint32_t              m_size;
        std::vector<uint32_t> m_dummy_data;
    };
}

extern "C" hltsv::L1Source *create_source(Configuration *config, const daq::df::RoIBPlugin *roib, const std::vector<std::string>& /* unused */)
{

    const daq::df::RoIBPluginInternal *my_config = config->cast<daq::df::RoIBPluginInternal>(roib);
    if(my_config == nullptr) {
        throw hltsv::ConfigFailed(ERS_HERE, "Invalid type for configuration to L1InternalSource");
    }
    return new hltsv::L1InternalSource(my_config);
}

namespace hltsv {

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

        if(m_hold) return nullptr;

        //create the ROB fragment 
        const uint32_t lvl1_type = 0xff;
  
        const uint32_t event_type = 0x0; // params->getLumiBlock(); ?????

        eformat::helper::SourceIdentifier src(eformat::TDAQ_CTP, 1);

        eformat::write::ROBFragment rob(src.code(), m_run_number, m_l1id, m_l1id,
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
    L1InternalSource::reset(uint32_t run_number)
    {
        // temporary hack to initialize L1ID 
        m_l1id = 0;
        m_lb   = 1;
        m_run_number = run_number;
        m_hold = 1;
    }

}
