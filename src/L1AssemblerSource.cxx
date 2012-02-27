#include <unistd.h>
#include <stdlib.h>
#include <iostream>
#include <algorithm>

#include "eformat/eformat.h"
#include "eformat/write/ROBFragment.h"

#include "config/Configuration.h"
#include "dal/Partition.h"
#include "DFdal/DFParameters.h"
#include "DFdal/DataFile.h"

#include "EventStorage/pickDataReader.h"     // universal reader
#include "dcmessages/LVL1Result.h"

#include "CTPfragment/CTPfragment.h"
#include "CTPfragment/CTPdataformat.h"

#include "Issues.h"
#include "L1AssemblerSource.h"


extern "C" hltsv::L1Source *create_source(const std::string& , Configuration& config)
{
    return new hltsv::L1AssemblerSource(config);
}

namespace hltsv {

    const unsigned int MAXUINT = 0XFFFFFFFF;

    L1AssemblerSource::L1AssemblerSource(Configuration& config)
        : m_l1id(0),
          m_config(config)
    {
    }

    L1AssemblerSource::~L1AssemblerSource()
    {
        m_data.clear();
    }

    dcmessages::LVL1Result* L1AssemblerSource::getResult()
    {
        assembler_task();

        // Create LVL1result, re-use ROBFragment
        dcmessages::LVL1Result* l1Result = 0;

        uint32_t* robs[dcmessages::MAXLVL1RODS];

        const std::vector<uint32_t*>& rob_frag = m_data;
      
        std::vector<uint32_t*>::const_iterator fit = rob_frag.begin();

        for ( size_t i=0; i< rob_frag.size(); i++, ++fit ) {
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
            }
            // --- bind and copy fragment back
            const eformat::write::node_t* toplist = wrob.bind();
            eformat::write::copy(*toplist, *fit, wrob.size_word());
            robs[i] = *fit;


            uint32_t* rob = robs[i];
            // Rewrite LB and HLT counter in CTP fragment
            const eformat::ROBFragment<const uint32_t*> rob_frag(const_cast<uint32_t*>(robs[i]));
            
            boost::mutex::scoped_lock lock(m_mutex);

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

            // mutex is unlocked here
        }

        if (rob_frag.size()) {
            l1Result = new dcmessages::LVL1Result( robs, rob_frag.size() );
        } else {
            std::ostringstream mesg;
            mesg <<"looking for LVL1 RoIs to build LVL1Result with l1id " <<
                m_l1id ;
            FileFailed issue(ERS_HERE,(mesg.str()).c_str());
            ers::warning(issue);
        }
        m_l1id += 1;
    //    std::cout << "m_l1id: " << m_l1id;
        if (m_data_770001.size() <= m_l1id) {
            m_l1id = m_firstid;
            m_modid = true;
        } else {
        }

        return l1Result;
    }

    void L1AssemblerSource::preset()
    {
        // clean any stored fragments
        std::map<unsigned int, std::vector<uint32_t*>*>::iterator mit;

        // also reset l1_id index
        m_l1id = 0;
        m_max_l1id = 0;
        m_modid = false;

        // empty the event map
        m_data.clear();

        const daq::core::Partition *partition = m_config.get<daq::core::Partition>(getenv("TDAQ_PARTITION"));
        const daq::df::DFParameters *dfparams = m_config.cast<daq::df::DFParameters>(partition->get_DataFlowParameters());

        const std::vector<const daq::df::DataFile*>& dataFiles = 
            dfparams->get_UsesDataFiles();

        std::vector<std::string> l1SourceFile; 
        for ( daq::df::DataFileIterator it = dataFiles.begin(); it != dataFiles.end(); it++) {
            l1SourceFile.push_back((*it)->get_FileName());
        }
        
        std::string file = l1SourceFile[0] + ".per-rob";

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
            for (std::vector<std::string>::const_iterator it = l1SourceFile.begin();
                 it != l1SourceFile.end(); it++) {
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

                        for ( size_t i=0; i<nrobs; i++) {
                            eformat::write::ROBFragment test_frag(const_cast<uint32_t*>(robs[i]));
    
                            uint32_t len = test_frag.size_word();
                            uint32_t*rob_frag = new uint32_t[len];

    
                            const eformat::write::node_t* list = test_frag.bind();
                            len = eformat::write::copy( list[0],
                                                        rob_frag,
                                                        len);
    

                            switch (test_frag.source_id()) {
                            case 0x00770001:
                              {
                                  m_data_770001[m_l1id] = rob_frag;
                                  break;
                              }
                            case 0x00760001:
                              {
                                  m_data_760001[m_l1id] = rob_frag;
                                  break;
                              }
                            case 0x007500AC:
                              {
                                  m_data_7500ac[m_l1id] = rob_frag;
                                  break;
                              }
                            case 0x007500AD:
                              {
                                  m_data_7500ad[m_l1id] = rob_frag;
                                  break;
                              }
                            case 0x007300A8:
                              {
                                  m_data_7300a8[m_l1id] = rob_frag;
                                  break;
                              }
                            case 0x007300A9:
                              {
                                  m_data_7300a9[m_l1id] = rob_frag;
                                  break;
                              }
                            case 0x007300AA:
                              {
                                  m_data_7300aa[m_l1id] = rob_frag;
                                  break;
                              }
                            case 0x007300AB:
                              {
                                  m_data_7300ab[m_l1id] = rob_frag;
                                  break;
                              }

                            default:
                                break;
                            }
    
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

        if (m_data_770001.empty()) {
            hltsv::FileFailed issue0(ERS_HERE, "looking for valid L1 results" );
            ers::error(issue0);
            hltsv::NoL1ResultsException issue(ERS_HERE, issue0);
            throw issue;
        }
        
        // only preloadedMod...
        m_max_l1id++;
    }

    void L1AssemblerSource::reset()
    {
        m_firstid = m_l1id = 0;
    } 

    void L1AssemblerSource::setLB(uint32_t lb)
    {
        boost::mutex::scoped_lock lock(m_mutex);
        m_lb = lb;
    }

    void L1AssemblerSource::setHLTCounter(uint16_t counter)
    {
        boost::mutex::scoped_lock lock(m_mutex);
        m_hltCounter = counter;
    }

    void L1AssemblerSource::assembler_task() {

        m_data.clear();

        m_data.push_back(m_data_770001[m_l1id]);
        m_data.push_back(m_data_760001[m_l1id]);
        m_data.push_back(m_data_7300a8[m_l1id]);
        m_data.push_back(m_data_7300a9[m_l1id]);
        m_data.push_back(m_data_7300aa[m_l1id]);
        m_data.push_back(m_data_7300ab[m_l1id]);
        m_data.push_back(m_data_7500ac[m_l1id]);
        m_data.push_back(m_data_7500ad[m_l1id]);
        
    }


}
