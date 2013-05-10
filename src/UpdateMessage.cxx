
#include "UpdateMessage.h"

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

    
}

