
#include "ClearMessage.h"
#include "LVL1Result.h"

#include "ers/ers.h"

namespace hltsv {
    
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

