
#include "ProcessMessage.h"
#include "LVL1Result.h"

namespace hltsv {
    
    // //////////////////////////////////////////////////
    // ProcessMessage
    // //////////////////////////////////////////////////
    ProcessMessage::ProcessMessage(std::shared_ptr<LVL1Result> rois)
        : m_rois(rois)
    {
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

