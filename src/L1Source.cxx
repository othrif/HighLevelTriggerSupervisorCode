
#include "L1Source.h"
#include "TTCInfo/LumiBlockNamed.h"
#include "RunControl/Common/OnlineServices.h"

namespace hltsv {

    L1Source::L1Source()
        : m_hold(0),
          m_lb(1),
          m_l1_prescale(),
          m_hlt_prescale(),
          m_lb_interval_seconds(120),
          m_lb_minimal_block_length_seconds(10),
          m_bunchgroup(),
          m_folder_index(),
          m_hlt_counter(1)
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
        ERS_LOG("Hold trigger");
        return m_hold;
    }

    void L1Source::resume() 
    {
        if(m_hold > 0) {
            m_hold -= 1;
        }
        ERS_LOG("Resume trigger");
    }

    void L1Source::setPrescales(uint32_t  l1p, uint32_t hltp, uint32_t /* lb */) 
    {
        m_l1_prescale = l1p;
        m_hlt_prescale = hltp;
        m_hlt_counter = m_lb + 1;
        m_cond.notify_one();
        ERS_LOG("Set prescales: " << l1p << ' ' << hltp)
    }

    void L1Source::setL1Prescales(uint32_t l1p) 
    {
        m_l1_prescale = l1p;
        ERS_LOG("Set L1 prescale: " << l1p);
    }

    void L1Source::setHLTPrescales(uint32_t hltp, uint32_t /* lb */) 
    {
        m_hlt_prescale = hltp;
        m_hlt_counter = m_lb + 1;
        m_cond.notify_one();
        ERS_LOG("Set HLT prescale: " << hltp );
    }

    void L1Source::increaseLumiBlock(uint32_t runno) 
    {
        m_lb += 1;
        m_cond.notify_one();
        ERS_LOG("Increase lumi block " << runno);
    }

    void L1Source::setLumiBlockInterval(uint32_t seconds) 
    {
        m_lb_interval_seconds = seconds;
        ERS_LOG("Set lumi block interval: " << seconds << " secs");
    }

    void L1Source::setMinLumiBlockLength(uint32_t seconds) 
    {
        m_lb_minimal_block_length_seconds = seconds;
        m_cond.notify_one();
        ERS_LOG("Set minimum lumi block length: " << seconds << " secs");
    }

    void L1Source::setBunchGroup(uint32_t bg) 
    {
        m_bunchgroup = bg;
        m_cond.notify_one();
        ERS_LOG("Set bunch group: " << bg);
    }

    void L1Source::setConditionsUpdate(uint32_t folderIndex, uint32_t /* lb */) 
    {
        m_folder_index = folderIndex;
        m_cond.notify_one();
        ERS_LOG("Set conditions: " << folderIndex);
    }

    void L1Source::startLumiBlockUpdate()
    {
        m_update = true;
        publish_lumiblock();

        m_thread = std::thread([this]() {
                while(m_update) {
                    std::unique_lock<std::mutex> lock(m_mutex);
                    m_cond.wait_for(lock, std::chrono::seconds(m_lb_interval_seconds));
                    if(m_update) {
                        m_lb += 1;
                        publish_lumiblock();
                    }
                }
            });
        m_thread.detach();
    }

    void L1Source::stopLumiBlockUpdate()
    {
        m_update = false;
        m_cond.notify_one();
    }

    void L1Source::publish_lumiblock()
    {
        LumiBlockNamed lumiblock(daq::rc::OnlineServices::instance().getIPCPartition(), "RunParams.LuminosityInfo");
        lumiblock.RunNumber = m_run_number;
        lumiblock.LumiBlockNumber = m_lb;
        lumiblock.Time = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        lumiblock.checkin();
    }

}
