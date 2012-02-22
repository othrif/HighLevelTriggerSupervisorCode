// this is -*- c++ -*-
#ifndef HLTSV_L1PRELOADEDSOURCE_H_
#define HLTSV_L1PRELOADEDSOURCE_H_

#include "L1Source.h"

#include <string>
#include <vector>
#include <map>
#include <stdint.h>

#include "boost/thread/mutex.hpp"

namespace hltsv {

    /**
     * The L1AssemblerSource class encapsulates the version of the
     * L1Source class which provides a LVL1Result object assembled
     * from preloaded rob fragments from a byte stream formatted
     * file as indicated in the configuration object.
     **/
    class L1AssemblerSource : public L1Source {
    public:
        L1AssemblerSource(Configuration& config);
        ~L1AssemblerSource();
        
        virtual dcmessages::LVL1Result* getResult();
        virtual void                    reset();
        virtual void                    preset();
        virtual void                    setLB(uint32_t lb);
        virtual void                    setHLTCounter(uint16_t counter);
        
    private:
        unsigned int  m_l1id;
        unsigned int  m_max_l1id;

        uint32_t      m_lb;
        uint16_t      m_hltCounter;
        
        int           m_nodes;
        std::string   m_nodeid;
        
        std::map<unsigned int, std::vector<uint32_t*>*> m_data;

        std::map<unsigned int, std::vector<uint32_t*>*> m_data_770001;
        std::map<unsigned int, std::vector<uint32_t*>*> m_data_760001;
        std::map<unsigned int, std::vector<uint32_t*>*> m_data_7300a8;
        std::map<unsigned int, std::vector<uint32_t*>*> m_data_7300a9;
        std::map<unsigned int, std::vector<uint32_t*>*> m_data_7300aa;
        std::map<unsigned int, std::vector<uint32_t*>*> m_data_7300ab;
        std::map<unsigned int, std::vector<uint32_t*>*> m_data_7500ac;
        std::map<unsigned int, std::vector<uint32_t*>*> m_data_7500ad;
        
        unsigned int  m_firstid;
        bool          m_modid;
        
        boost::mutex  m_mutex; 

        Configuration& m_config;

        void assembler_task();
    };
}

#endif // HLTSV_L1PRELOADEDSOURCE_H_
