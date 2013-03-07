
#include "Messages.h"

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
    
}
