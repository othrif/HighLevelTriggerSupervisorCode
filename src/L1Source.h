// this is -*- c++ -*-
#ifndef HLTSV_L1SOURCE_H_
#define HLTSV_L1SOURCE_H_

#include <stdint.h>
#include <string>
#include <vector>

namespace hltsv {

    class LVL1Result;

    /**
     * Abstract base class for interface to specfic sources
     * of LVL1Result objects.
     *
     */
    class L1Source {
    public:

        /** 
         * \brief Abstract interfact to RoIBuilder.
         * 
         * \param[in] source_type The name of the plugin.
         * \param[in] file_names The list of file names for the pre-loaded mode.
         *
         * Each implementation must provide a function with
         * this signature named "create_source(source_type, arguments)"  that
         * will be called to instantiate the interface.
         *
         */
        
        typedef L1Source *(*creator_t)(const std::string& source_type, const std::vector<std::string>& file_names);

        virtual ~L1Source();

    public:    
        /** Try to get an event from LVL1.
         *  @returns A pointer to the event, NULL if none available.
         */
        virtual LVL1Result* getResult(void) = 0;
        
        /** Executed in PrepareForRun */
        virtual void                    reset(); 
        
        /** Executed in Configure */
        virtual void                    preset();
        
        virtual void                    setLB(uint32_t);
        virtual void                    setHLTCounter(uint16_t);
        
    };
    
}

#endif // HLTSV_L1SOURCE_H_
