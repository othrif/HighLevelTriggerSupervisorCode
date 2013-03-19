// this -*- c++ -*-
#ifndef HLTSV_ACTIVITY_H_
#define HLTSV_ACTIVITY_H_

// Run control related
#include "RunController/Controllable.h"
#include "RunController/CommandedTrigger.h"
#include "RunController/MasterTrigger.h"

// Monitoring
#include "TH1F.h"
#include "TFile.h"
#include "HLTSV.h"

#include "monsvc/ptr.h"
#include "monsvc/PublishingController.h"

// for dynamic loading of L1Source
namespace daq { 
    namespace dynlibs {
        class DynamicLibrary;
    }
}

namespace hltsv {

    class L1Source;
    class EventScheduler;
    class ROSClear;

    //
    // Inheriting from MasterTrigger and daq::rc::Controllable at
    // the same time is not possbible, since they have conflicting resume()
    // methods.
    // 
    class MasterTrigger : public daq::rc::MasterTrigger {
    public:
        MasterTrigger(L1Source *l1source, bool& triggering);
        uint32_t hold();
        void resume();
        void setL1Prescales(uint32_t l1p);
        void setHLTPrescales(uint32_t hltp, uint32_t lb);
        void setPrescales(uint32_t  l1p, uint32_t hltp, uint32_t lb);
        void setLumiBlock(uint32_t lb, uint32_t runno);
        void setBunchGroup(uint32_t bg);
        void setConditionsUpdate(uint32_t folderIndex, uint32_t lb);
    private:
        L1Source *m_l1source;
        bool&    m_triggering;
        int      m_triggerHoldCounter;
    };

    class Activity : public daq::rc::Controllable {
    public:
        Activity(const std::string&);
        ~Activity();
    
        // Run control commands
        void configure(std::string&);
        void connect(std::string&);
        void prepareForRun(std::string&);
        void disable(std::string&);
        void enable(std::string&);
        void stopL2SV(std::string&);
        void unconfigure(std::string&);
        void disconnect(std::string&);

    private:

        // for MasterTrigger interface
        std::unique_ptr<MasterTrigger>              m_master_trigger;
        std::unique_ptr<daq::rc::CommandedTrigger>  m_cmdReceiver;
    
        // L1 Source
        daq::dynlibs::DynamicLibrary    *m_l1source_lib;
        hltsv::L1Source                 *m_l1source;

        // Monitoring
        hltsv::HLTSV                    m_stats;
        monsvc::ptr<TH1F>               m_time;

        std::unique_ptr<monsvc::PublishingController> m_publisher;
        std::unique_ptr<TFile>                        m_outfile;
    
        // Running flags
        bool                            m_network;
        bool                            m_running;
        bool                            m_triggering;

        unsigned int                    m_timeout;
    };
}

#endif // HLTSV_ACTIVITY_H_

