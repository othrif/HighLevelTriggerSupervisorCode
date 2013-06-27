// this is -*- c++ -*-
#ifndef HLTSV_L1SOURCE_H_
#define HLTSV_L1SOURCE_H_

#include <stdint.h>
#include <string>
#include <vector>

class Configuration;

namespace daq {
    namespace df {
        class RoIBPlugin;
    }
}

namespace hltsv {

    class LVL1Result;

    /// The maximum number of possible RODs in the RoIB data.
    static const size_t MAXLVL1RODS = 12;

    /**
     * \brief Abstract base class for interface to specfic sources
     * of LVL1Result objects.
     *
     */
    class L1Source {
    public:

        /** 
         * \brief Abstract interfact to RoIBuilder.
         * 
         * \param[in] roib A pointer to the RoIBPlugin object from OKS.
         * \param[in] file_names The list of file names for the pre-loaded mode.
         *
         * Each implementation must provide a function with
         * this signature named "create_source(source_type, arguments)"  that
         * will be called to instantiate the interface.
         *
         */
        
        typedef L1Source *(*creator_t)(Configuration *config, const daq::df::RoIBPlugin *roib, const std::vector<std::string>& file_names);

        virtual ~L1Source();

    public:

        /** Try to get an event from LVL1.
         *  @returns A pointer to the event, NULL if none available.
         */
        virtual LVL1Result* getResult(void) = 0;
        
        /** Executed in PrepareForRun */
        virtual void                    reset(uint32_t run_number); 
        
        /** Executed in Configure */
        virtual void                    preset();

        /**  Called from the MasterTrigger interface. */
        virtual void                    setLB(uint32_t);
        virtual void                    setHLTCounter(uint16_t);
        
    };
    
}

#endif // HLTSV_L1SOURCE_H_
