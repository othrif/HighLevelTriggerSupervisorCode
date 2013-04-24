
#include "Messages.h"
#include "LVL1Result.h"

#include "ers/ers.h"

namespace hltsv {
    
    // 
    // Update Message
    // 
    UpdateMessage::UpdateMessage(size_t size)
        : m_data(size/sizeof(uint32_t))
    {
    }

    UpdateMessage::~UpdateMessage()
    {
    }

    uint32_t UpdateMessage::typeId() const
    {
        return ID;
    }

    uint32_t UpdateMessage::transactionId() const 
    {
        // don't care
        return 0;
    }

    void UpdateMessage::toBuffers(std::vector<boost::asio::mutable_buffer>& buffers) 
    {
        buffers.push_back(boost::asio::buffer(m_data));
    }

    uint32_t UpdateMessage::num_request() const
    {
        return m_data[0];
    }

    size_t UpdateMessage::num_l1ids() const
    {
        return m_data.size() - 1;
    }

    uint32_t UpdateMessage::l1id(size_t index) const
    {
        return m_data[index+1];
    }

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

    //
    // ClearMessage
    // 

    ClearMessage::ClearMessage(uint32_t sequence, std::shared_ptr<std::vector<uint32_t>> events)
        : m_sequence(sequence),
          m_events(events)
    {
    }

    ClearMessage::~ClearMessage()
    {
    }

    uint32_t ClearMessage::typeId()        const
    {
        return ID;
    }

    uint32_t ClearMessage::transactionId() const 
    {
        return m_sequence;
    }

    void  ClearMessage::toBuffers(std::vector<boost::asio::const_buffer>& bufs) const
    {
        bufs.push_back(boost::asio::buffer(*m_events));
    }

    
}

