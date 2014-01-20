
#include "L1Source.h"

namespace hltsv {

    L1Source::L1Source()
        : m_hold(0),
          m_lb(1),
          m_l1_prescale(),
          m_hlt_prescale(),
          m_lb_interval_seconds(120),
          m_lb_minimal_block_length_seconds(10),
          m_bunchgroup(),
          m_folder_index()
    {
    }

    L1Source::~L1Source()
    {
    }

    void L1Source::reset(uint32_t)
    {
    }

    void L1Source::preset()
    {
    }

    uint32_t L1Source::hold()
    {
        m_hold += 1;
        return m_hold;
    }

    void L1Source::resume() 
    {
        if(m_hold > 0) {
            m_hold -= 1;
        }
    }

    void L1Source::setPrescales(uint32_t  l1p, uint32_t hltp, uint32_t lb) 
    {
        m_l1_prescale = l1p;
        m_hlt_prescale = hltp;
        m_lb = lb;
    }

    void L1Source::setL1Prescales(uint32_t l1p) 
    {
        m_l1_prescale = l1p;
    }

    void L1Source::setHLTPrescales(uint32_t hltp, uint32_t lb) 
    {
        m_hlt_prescale = hltp;
        m_lb = lb;
    }

    void L1Source::increaseLumiBlock(uint32_t /* runno */) 
    {
        m_lb += 1;
    }

    void L1Source::setLumiBlockInterval(uint32_t seconds) 
    {
        m_lb_interval_seconds = seconds;
    }

    void L1Source::setMinLumiBlockLength(uint32_t seconds) 
    {
        m_lb_minimal_block_length_seconds = seconds;
    }

    void L1Source::setBunchGroup(uint32_t bg) 
    {
        m_bunchgroup = bg;
    }

    void L1Source::setConditionsUpdate(uint32_t folderIndex, uint32_t lb) 
    {
        m_folder_index = folderIndex;
        m_lb = lb;
    }

    void L1Source::startLumiBlockUpdate()
    {
        m_update = true;
        m_thread = std::thread([this]() {
                while(m_update) {
                    std::this_thread::sleep_for(std::chrono::seconds(m_lb_interval_seconds));
                    m_lb += 1;
                }
            });
        m_thread.detach();
    }

    void L1Source::stopLumiBlockUpdate()
    {
        m_update = false;
    }

}
