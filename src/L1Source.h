// this is -*- c++ -*-
#ifndef HLTSV_L1SOURCE_H_
#define HLTSV_L1SOURCE_H_

#include <stdint.h>
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <mutex>

#include "TriggerCommander/MasterTrigger.h"

class Configuration;

namespace daq {
    namespace df {
        class RoIBPlugin;
    }
}

namespace hltsv {

    class LVL1Result;
    class HLTSV;

    /// The maximum number of possible RODs in the RoIB data.
    static const size_t MAXLVL1RODS = 12;

    /**
     * \brief Abstract base class for interface to specfic sources
     * of LVL1Result objects.
     *
     */
    class L1Source : public daq::trigger::MasterTrigger {
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

        L1Source();
        virtual ~L1Source();

    public:

        /** Try to get an event from LVL1.
         *  @returns A pointer to the event, NULL if none available.
         */
        virtual LVL1Result*             getResult(void) = 0;
        
        /** Executed in PrepareForRun */
        virtual void                    reset(uint32_t run_number); 
        
        /** Executed in Configure */
        virtual void                    preset();

        /** Fill in monitoring information on demand.
         * 
         *  The default implementation does nothing.
         *  Extend the schema/hltsv_is.schema.xml class with appropriate
         *  attributes for the plugin and fill them inside this method.
         *  This method is called from a separated monitoring thread.
         */
        virtual void                    getMonitoringInfo(HLTSV *info);

        /** MasterTrigger interface.
         *
         *  All of these have a a default implementation.
         */
        uint32_t hold(const std::string& /* mask */) override;
        void resume(const std::string& /* mask */) override;
        void setPrescales(uint32_t  l1p, uint32_t hltp) override;
        void setL1Prescales(uint32_t l1p) override;
        void setHLTPrescales(uint32_t hltp) override;
        void increaseLumiBlock(uint32_t runno) override;
        void setLumiBlockInterval(uint32_t seconds) override;
        void setMinLumiBlockLength(uint32_t seconds) override;
        void setBunchGroup(uint32_t bg) override;
        void setConditionsUpdate(uint32_t folderIndex, uint32_t lb) override;

        void startLumiBlockUpdate();
        void stopLumiBlockUpdate();

    protected:
        /**
         * These methods and variables are used to communicate the MasterTrigger
         * state to the subclasses.
         */

        void publish_lumiblock();

        std::atomic<uint32_t> m_hold;
        std::atomic<uint32_t> m_lb;

        std::atomic<uint32_t> m_l1_prescale;
        std::atomic<uint32_t> m_hlt_prescale;

        std::atomic<uint32_t> m_lb_interval_seconds;
        std::atomic<uint32_t> m_lb_minimal_block_length_seconds;
        
        uint32_t m_bunchgroup;

        std::atomic<uint32_t> m_folder_index;

        // Set to lb on hlt prescale update
        std::atomic<uint32_t> m_hlt_counter;

        // set by subclass
        std::atomic<uint32_t> m_run_number;

        // for update thread
        std::atomic<bool>       m_update;
        std::mutex              m_mutex;
        std::condition_variable m_cond;
        std::thread m_thread;

        static std::vector<uint32_t> s_ctp_template;

    };
    
}

#endif // HLTSV_L1SOURCE_H_
