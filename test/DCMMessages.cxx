
#include "DCMMessages.h"
#include "ers/ers.h"


namespace hltsv {

  RequestMessage::RequestMessage(uint32_t reqRoIs, const std::vector<uint32_t>& l1ids)
    : m_l1ids(l1ids)
  {
    m_prefix.reqRoIs = reqRoIs;
    m_prefix. numl1ids = l1ids.size();
  }
  
  RequestMessage::~RequestMessage()
  {
  }
  
  uint32_t RequestMessage::typeId() const
  {
    return ID;
  }
  
  uint32_t RequestMessage::transactionId() const 
  {
    return 0;
  }
  
  void RequestMessage::toBuffers(std::vector<boost::asio::const_buffer>& buffers) const
  {
    ERS_LOG("RequestMessage::tobuffer called, "<< m_prefix.reqRoIs);
    buffers.push_back(boost::asio::buffer(&m_prefix, sizeof(m_prefix)));
    buffers.push_back(boost::asio::buffer(m_l1ids));
  }
   
  // ******

    AssignMessage::AssignMessage(size_t size)
        : m_data(new uint32_t[size/sizeof(uint32_t) + 1]),
          m_size(size)
    {
    }

    AssignMessage::~AssignMessage()
    {
    }

    uint32_t AssignMessage::typeId() const
    {
        // will be hard-coded
        return 1;
    }

    uint32_t AssignMessage::transactionId() const 
    {
        // don't care
        return 0;
    }

    void AssignMessage::toBuffers(std::vector<boost::asio::mutable_buffer>& buffers) 
    {
      ERS_LOG("AssignMessage::toBuffers");
      buffers.push_back(boost::asio::buffer(m_data.get(), m_size));
    }


} 
