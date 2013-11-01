#include <string>
#include <vector>
#include <map>
#include <stdint.h>
#include <mutex>

#include <unistd.h>
#include <stdlib.h>
#include <iostream>
#include <algorithm>

#include "eformat/eformat.h"
#include "eformat/write/ROBFragment.h"

#include "EventStorage/pickDataReader.h"     // universal reader


#include "CTPfragment/CTPfragment.h"
#include "CTPfragment/CTPdataformat.h"

#include "Issues.h"
#include "LVL1Result.h"
#include "L1Source.h"

#include "config/Configuration.h"
#include "DFdal/RoIBPluginPreload.h"

namespace hltsv {

    /**
     * \brief The L1PreloadedSource class encapsulates the version of the
     * L1Source class which provides a LVL1Result object from data
     * preloaded from a byte stream formatted file as indicated in
     * the configuration object.
     *
     */
    class L1PreloadedSource : public L1Source {
    public:
        L1PreloadedSource(const std::vector<std::string>& file_names);
        ~L1PreloadedSource();
        
        virtual LVL1Result* getResult() override;
        virtual void        reset(uint32_t run_number) override;
        virtual void        preset() override;
        virtual void        setLB(uint32_t lb) override;
        virtual void        setHLTCounter(uint16_t counter) override;
        
    private:
        unsigned int  m_l1id;
        unsigned int  m_max_l1id;

        uint32_t      m_lb;
        uint16_t      m_hltCounter;
        uint32_t      m_run_number;
        
        std::map<unsigned int, std::vector<uint32_t*>*> m_data;
        
        unsigned int  m_firstid;
        bool          m_modid;
        
        std::mutex    m_mutex; 

        std::vector<std::string> m_file_names;
    };
}

extern "C" hltsv::L1Source *create_source(Configuration *config, const daq::df::RoIBPlugin *roib, const std::vector<std::string>& file_names)
{
    const daq::df::RoIBPluginPreload *my_config = config->cast<daq::df::RoIBPluginPreload>(roib);
    if(my_config == nullptr) {
        throw hltsv::ConfigFailed(ERS_HERE, "Invalid type for configuration to L1PreloadedSource");
    }
    return new hltsv::L1PreloadedSource(file_names);
}

namespace hltsv {

    const unsigned int MAXUINT = 0XFFFFFFFF;

    L1PreloadedSource::L1PreloadedSource(const std::vector<std::string>& file_names)
        : m_l1id(0),
          m_file_names(file_names)
    {
        if(m_file_names.empty()) {
            throw hltsv::ConfigFailed(ERS_HERE, "Empty list of file names for L1PreloadedSource");
        }
    }

    L1PreloadedSource::~L1PreloadedSource()
    {
        // cleanup
        std::map<unsigned int, std::vector<uint32_t*>*>::iterator mit;

        for ( mit=m_data.begin(); mit!=m_data.end(); mit++ ) {
            std::vector<uint32_t*>* rob_frag = (*mit).second;
            std::vector<uint32_t*>::iterator fit;
            for ( fit=rob_frag->begin(); fit!=rob_frag->end(); fit++ ) {
                delete [] (*fit);
            }
            delete (*mit).second;
        }
    }

    LVL1Result* L1PreloadedSource::getResult()
    {
        // Create LVL1result, re-use ROBFragment
        LVL1Result* l1Result = 0;

        uint32_t* robs[MAXLVL1RODS]; 
        uint32_t  lengths[MAXLVL1RODS];

        std::vector<uint32_t*>* rob_frag = m_data[m_l1id];
      
        std::vector<uint32_t*>::iterator fit = rob_frag->begin();

        uint32_t lvl1_id = 0;

        for ( size_t i=0; i< rob_frag->size(); i++, fit++ ) {
            /**
             * Update Lumi Block Number in Rob's header
             */
            uint32_t event_type = 1; /// ??? params->getLumiBlock();
            // --- copy a writable ROB wrob
            eformat::write::ROBFragment wrob = *fit;
            // --- set event_type
            wrob.rod_detev_type(event_type);


            // --- set event type with Lumi Block from L2SVP
            if ( m_max_l1id > 0 && m_modid ) {
                // eformat::write::ROBFragment test_frag(const_cast<uint32_t*>(robs[i]));
                uint32_t old_l1id = wrob.rod_lvl1_id();
                // if ( (MAXUINT - test_frag.rod_lvl1_id()) > m_max_l1id )
                if ( (MAXUINT - old_l1id) > m_max_l1id ) {
                    // test_frag.rod_lvl1_id( test_frag.rod_lvl1_id() + m_max_l1id );
                    wrob.rod_lvl1_id(old_l1id + m_max_l1id);
                } else {
                    // test_frag.rod_lvl1_id( test_frag.rod_lvl1_id() % m_max_l1id );
                    wrob.rod_lvl1_id(old_l1id % m_max_l1id);
                }
                wrob.rod_run_no(m_run_number);
            }
            // --- bind and copy fragment back
            const eformat::write::node_t* toplist = wrob.bind();
	    robs[i] = new uint32_t[wrob.size_word()];
            lengths[i] = wrob.size_word();
            eformat::write::copy(*toplist, robs[i], wrob.size_word());

            uint32_t* rob = robs[i];
            // Rewrite LB and HLT counter in CTP fragment
            const eformat::ROBFragment<const uint32_t*> rob_frag(const_cast<uint32_t*>(robs[i]));
            
            std::lock_guard<std::mutex> lock(m_mutex);

            if ( rob_frag.source_id()==0x00770001 ) {    // CTP fragment
                if ( CTPfragment::ctpFormatVersion(&rob_frag) == 0 ) {
                    // test_frag.rod_detev_type(...)
                    rob[16] = ((m_lb & CTPdataformat::LumiBlockMask) << CTPdataformat::LumiBlockShift) |
                        ((m_hltCounter & CTPdataformat::HltCounterMask_v0) << CTPdataformat::HltCounterShift_v0);
                }
                else {
                    rob[16] = ((m_lb & CTPdataformat::LumiBlockMask) << CTPdataformat::LumiBlockShift) |
                        ((m_hltCounter & CTPdataformat::HltCounterMask_v1) << CTPdataformat::HltCounterShift_v1);
                }
            }

            if(i == 0) {
                lvl1_id = wrob.rod_lvl1_id();
            }

            // mutex is unlocked here
        }

        if (rob_frag->size()) {
	    l1Result = new LVL1Result( lvl1_id, rob_frag->size(), robs, lengths);
        } else {
            std::ostringstream mesg;
            mesg <<"looking for LVL1 RoIs to build LVL1Result with l1id " <<
                m_l1id ;
            FileFailed issue(ERS_HERE,(mesg.str()).c_str());
            ers::warning(issue);
        }
        m_l1id += 1;
        if (m_data.size() <= m_l1id) {
            m_l1id = m_firstid;
            m_modid = true;
        }

        return l1Result;
    }

    void L1PreloadedSource::preset()
    {
        // clean any stored fragments
        std::map<unsigned int, std::vector<uint32_t*>*>::iterator mit;
        for ( mit=m_data.begin(); mit!=m_data.end(); mit++ ) {
            std::vector<uint32_t*>* rob_frag = (*mit).second;
            std::vector<uint32_t*>::iterator fit;
            for ( fit=rob_frag->begin(); fit!=rob_frag->end(); fit++ ) {
                delete [] (*fit);
            }
            delete (*mit).second;
        }

        // also reset l1_id index
        m_l1id = 0;
        m_max_l1id = 0;
        m_run_number = 0;
        m_modid = false;

        // empty the event map
        m_data.erase(m_data.begin(), m_data.end());

        std::string file = m_file_names[0] + ".per-rob";

        if ((access(file.c_str(), R_OK | X_OK)) == -1) {
            for (std::vector<std::string>::const_iterator it = m_file_names.begin();
                 it != m_file_names.end(); it++) {
                //Open the file and read-in event per event
                DataReader *pDR = pickDataReader(*it);

                if(!pDR) {
                    std::ostringstream mesg;
                    mesg<<"opening data file " << *it;
                    hltsv::FileFailed issue(ERS_HERE,(mesg.str()).c_str());
                    ers::error(issue);
                    break;
                }

                if(!pDR->good()) {
                    std::ostringstream mesg;
                    mesg<<"reading data file " << pDR->fileName();
                    hltsv::FileFailed issue(ERS_HERE,(mesg.str()).c_str());
                    ers::error(issue);
                    break;
                }

                while(pDR->good()) {
                    unsigned int eventSize;    
                    char *buf;
	
                    DRError ecode = pDR->getData(eventSize,&buf);

                    while(DRWAIT == ecode) {// wait for more data to come           
                        usleep(500000);
                        ecode = pDR->getData(eventSize,&buf);
                    }

                    if(DRNOOK == ecode) {
                        break; 
                    }
	  
                    eformat::FullEventFragment<const uint32_t*>
                        fe(reinterpret_cast<const uint32_t*>(buf));

                    // get ROBFragments
                    const unsigned int MAX_ROB_COUNT=2048;
                    const uint32_t* robs[MAX_ROB_COUNT];
                    size_t nrobs = fe.children(robs, MAX_ROB_COUNT);
	
                    m_data[m_l1id] = new std::vector<uint32_t*>;
                    for ( size_t i=0; i<nrobs; i++) {
                        eformat::write::ROBFragment test_frag(const_cast<uint32_t*>(robs[i]));
	  
                        switch (test_frag.source_id()) {
                        case 0x00770001:
                        case 0x00760001:
                        case 0x007500AC:
                        case 0x007500AD:
                        case 0x007300A8:
                        case 0x007300A9:
                        case 0x007300AA:
                        case 0x007300AB:
                            {
                                uint32_t len = test_frag.size_word();
                                uint32_t*rob_frag = new uint32_t[len];
	      
                                const eformat::write::node_t* list = test_frag.bind();
                                len = eformat::write::copy( list[0],
                                                            rob_frag,
                                                            len);
	      
                                m_data[m_l1id]->push_back(rob_frag);

                                if (test_frag.rod_lvl1_id() > m_max_l1id)
                                    m_max_l1id = test_frag.rod_lvl1_id();
                            }
                            break;
	    
                        default:
                            break;
                        }
                    }
                    m_l1id++;
                    delete[] buf;
                }
                delete pDR;
            }
        } else {

            std::vector<std::string> l1_frag;
            l1_frag.push_back("0x00770001");
            l1_frag.push_back("0x00760001");
            l1_frag.push_back("0x007500AC");
            l1_frag.push_back("0x007500AD");
            l1_frag.push_back("0x007300A8");
            l1_frag.push_back("0x007300A9");
            l1_frag.push_back("0x007300AA");
            l1_frag.push_back("0x007300AB");

            // Read through data files, looking for LVL1 ROBFragments
            for ( size_t fit=0; fit<l1_frag.size(); fit++) {
                m_l1id=0;
                for (std::vector<std::string>::const_iterator it = m_file_names.begin();
                     it != m_file_names.end(); it++) {
                    std::string fileName = (*it) + ".per-rob/" + l1_frag[fit] + ".data";
	
                    //Open the file and read-in event per event
                    DataReader *pDR = pickDataReader(fileName);
                    ERS_DEBUG(2, "Loading data file " << fileName);

                    if(!pDR) {
                        std::ostringstream mesg;
                        mesg<<"opening data file " << fileName;
                        hltsv::FileFailed issue(ERS_HERE,(mesg.str()).c_str());
                        ers::error(issue);
                        break;
                    }

                    if(!pDR->good()) break;

                    while(pDR->good()) {
                        unsigned int eventSize;    
                        char *buf;
	  
                        DRError ecode = pDR->getData(eventSize,&buf);
	  
                        while(DRWAIT == ecode) {// wait for more data to come 
                            usleep(500000);
                            ecode = pDR->getData(eventSize,&buf);
                        }

                        if(DRNOOK == ecode) break; 
                
                        const size_t MAX_ROBS = 256;
                        const uint32_t* robs[MAX_ROBS];
                        size_t nrobs =
                            eformat::get_robs(reinterpret_cast<const uint32_t*>(buf), robs, MAX_ROBS);
	  
                        if (nrobs > 0) {

                            if (fit == 0) m_data[m_l1id] = new std::vector<uint32_t*>;
                            for ( size_t i=0; i<nrobs; i++) {
                                eformat::write::ROBFragment test_frag(const_cast<uint32_t*>(robs[i]));
	      
                                uint32_t len = test_frag.size_word();
                                uint32_t*rob_frag = new uint32_t[len];
	      
                                const eformat::write::node_t* list = test_frag.bind();
                                len = eformat::write::copy( list[0],
                                                            rob_frag,
                                                            len);
	      
                                m_data[m_l1id]->push_back(rob_frag);
	      
                                if (test_frag.rod_lvl1_id() > m_max_l1id) {
                                    m_max_l1id = test_frag.rod_lvl1_id();
                                }
                            }
                            m_l1id++;
                        }
                        delete[] buf;
                    }
                    delete pDR;
                }
            }
        }

        if (m_data.empty()) {
            hltsv::FileFailed issue0(ERS_HERE, "looking for valid L1 results" );
            ers::error(issue0);
            hltsv::NoL1ResultsException issue(ERS_HERE, issue0);
            throw issue;
        }
        
        // only preloadedMod...
        m_max_l1id++;
    }

    void L1PreloadedSource::reset(uint32_t run_number)
    {
        m_firstid = m_l1id = 0;
        m_run_number = run_number;
    } 

    void L1PreloadedSource::setLB(uint32_t lb)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lb = lb;
    }

    void L1PreloadedSource::setHLTCounter(uint16_t counter)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_hltCounter = counter;
    }


}
