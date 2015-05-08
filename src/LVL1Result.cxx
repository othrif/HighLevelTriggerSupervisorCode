
#include "ers/ers.h"
#include "LVL1Result.h"
#include "eformat/util.h"
#include "eformat/SourceIdentifier.h"
#include "L1Source.h"
#include "Issues.h"

// #include "eformat/eformat.h"
// #include "eformat/write/eformat.h"

namespace hltsv {

    LVL1Result::LVL1Result(uint32_t *rod_data, uint32_t rod_length)
        : m_lvl1_id(),
          m_deleter([](uint32_t *) {}),
          m_reassigned(false),
          m_converted(false),
          m_rod_data(rod_data),
          m_rod_length(rod_length)
    {
    }

    LVL1Result::~LVL1Result()
    {
        for(auto ptr : m_data) m_deleter(ptr);
        for(auto wr : m_writers) delete wr;
        delete [] m_rod_data;
    }

    uint32_t LVL1Result::l1_id() const
    {
        return m_lvl1_id;
    }

    bool     LVL1Result::reassigned() const
    {
        return m_reassigned;
    }

    void     LVL1Result::set_reassigned()
    {
        m_reassigned = true;
    }

    LVL1Result::time_point LVL1Result::timestamp() const
    {
        return m_time_stamp;
    }

    void LVL1Result::set_timestamp(time_point ts)
    {
        m_time_stamp = ts;
    }

    uint64_t   LVL1Result::global_id() const
    {
        return m_global_id;
    }
    
    void LVL1Result::set_global_id(uint64_t global_id)
    {
        m_global_id = global_id;
    }

    size_t LVL1Result::event_data_size() const
    {
        size_t result = 0;
        for(auto len : m_lengths) {
            result += len * sizeof(uint32_t);
        }
        return result;
    }

    void LVL1Result::copy(uint32_t *target) const
    {
        for(size_t i = 0; i < m_data.size(); i++) {
            memcpy(target, m_data[i], m_lengths[i] * sizeof(uint32_t));
            target += m_lengths[i];
        }
    }

    bool LVL1Result::create_rob_data()
    {
        if(!m_converted) {

            try {
                // locate the ROD fragments
                const uint32_t* rod[MAXLVL1RODS];
                uint32_t        rodsize[MAXLVL1RODS];

                bool found_ctp = false;

                uint32_t num_frags = eformat::find_rods(m_rod_data, m_rod_length, rod, rodsize, MAXLVL1RODS);

                // create the ROB fragments out of ROD fragments
                // eformat::write::ROBFragment* writers[MAXLVL1RODS];
                for (size_t i = 0; i < num_frags; ++i) {
                    m_writers.push_back(new eformat::write::ROBFragment(const_cast<uint32_t*>(rod[i]), rodsize[i]));

                    //update ROB header
                    m_writers[i]->rob_source_id(m_writers[i]->rod_source_id());
                    if(eformat::helper::SourceIdentifier(m_writers[i]->rob_source_id()).subdetector_id() == eformat::TDAQ_CTP) {
                        m_lvl1_id = m_writers[i]->rod_lvl1_id();
                        found_ctp = true;
                    } else if(!found_ctp) {
                        m_lvl1_id = m_writers[i]->rod_lvl1_id();
                    } // else CTP fragment already seen.
                }

                // make one single buffer out of the whole data
                for (size_t i = 0; i< num_frags; ++i) {
                    auto writer_list = m_writers[i]->bind();
                    while(writer_list) {
                        m_data.push_back(writer_list->base);
                        m_lengths.push_back(writer_list->size_word);
                        writer_list = writer_list->next;
                    }
                }

                m_converted = true;

                if(!found_ctp) {
                    hltsv::NoCTPFragment err(ERS_HERE);
                    ers::error(err);
                }

            } catch (eformat::Issue &e) {
                ers::error(e); 
                m_converted = false;
            } catch(std::exception& e) {
                ERS_LOG("Unexpected std::exception " << e.what());
                m_converted = false;
            } catch (...) {
                ERS_LOG("Unknwown exception");
                m_converted = false;
            }
        }

        return m_converted;
    }

    const uint32_t* LVL1Result::rod_data() const
    {
        return m_rod_data;
    }

    uint32_t LVL1Result::rod_length() const
    {
        return m_rod_length;
    }

}

