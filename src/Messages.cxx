
#include "Messages.h"
#include "LVL1Result.h"

namespace hltsv {
    
    UpdateMessage::UpdateMessage(size_t size)
        : m_data(new uint32_t[size/sizeof(uint32_t) + 1]),
          m_size(size)
    {
    }

    UpdateMessage::~UpdateMessage()
    {
    }

    uint32_t UpdateMessage::typeId() const
    {
        // will be hard-coded
        return 1;
    }

    uint32_t UpdateMessage::transactionId() const 
    {
        // don't care
        return 0;
    }

    void UpdateMessage::toBuffers(std::vector<boost::asio::mutable_buffer>& buffers) 
    {
        buffers.push_back(boost::asio::buffer(m_data.get(), m_size));
    }
    
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

    ClearMessage::ClearMessage(uint32_t sequence, std::shared_ptr<std::vector<uint32_t>> events)
        : m_events(events)
    {
        m_sequence_count[0] = sequence;
        m_sequence_count[1] = static_cast<uint32_t>(events->size());
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
        return 0;
    }

    void  ClearMessage::toBuffers(std::vector<boost::asio::const_buffer>& bufs) const
    {
        bufs.push_back(boost::asio::buffer(m_sequence_count));
        bufs.push_back(boost::asio::buffer(*m_events));
    }

    
}

