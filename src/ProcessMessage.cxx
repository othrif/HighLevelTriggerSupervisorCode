
#include "ProcessMessage.h"
#include "LVL1Result.h"

namespace hltsv {
    
    // //////////////////////////////////////////////////
    // ProcessMessage
    // //////////////////////////////////////////////////
    ProcessMessage::ProcessMessage(std::shared_ptr<LVL1Result> rois)
        : m_rois(rois)
    {
        m_prefix.global_id = rois->global_id();
        m_prefix.l1_id     = rois->l1_id();
    }

    ProcessMessage::~ProcessMessage()
    {
    }

    uint32_t ProcessMessage::typeId() const
    {
        return ID;
    }

    uint32_t ProcessMessage::transactionId() const
    {
        return 0;
    }

    void  ProcessMessage::toBuffers(std::vector<boost::asio::const_buffer>& buffers) const 
    {
      buffers.push_back(boost::asio::buffer(&m_prefix, sizeof(m_prefix)));
      m_rois->insert(buffers);
    }

    BuildMessage::BuildMessage(std::shared_ptr<LVL1Result> rois)
        : ProcessMessage(rois)
    {
    }

    uint32_t BuildMessage::typeId() const
    {
        return ID;
    }
    
}

