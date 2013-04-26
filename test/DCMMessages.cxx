
#include "DCMMessages.h"
#include "ers/ers.h"


namespace hltsv {

  RequestMessage::RequestMessage(uint32_t reqRoIs, const std::vector<uint32_t>& l1ids)
    : m_l1ids(l1ids)
  {
      m_reqRoIs = reqRoIs;
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
    buffers.push_back(boost::asio::buffer(&m_reqRoIs, sizeof(m_reqRoIs)));
    buffers.push_back(boost::asio::buffer(m_l1ids));
  }
   
  // ******

    AssignMessage::AssignMessage(size_t size)
        : m_data_size(size - 3*sizeof(uint32_t)),
          m_data(new uint32_t[m_data_size/sizeof(uint32_t) + 1])
    {
    }

    AssignMessage::~AssignMessage()
    {
    }

    uint32_t AssignMessage::typeId() const
    {
        // will be hard-coded
        return ID;
    }

    uint32_t AssignMessage::transactionId() const 
    {
        // don't care
        return 0;
    }

    void AssignMessage::toBuffers(std::vector<boost::asio::mutable_buffer>& buffers) 
    {
      buffers.push_back(boost::asio::buffer(&m_global_id, sizeof(uint64_t)));
      buffers.push_back(boost::asio::buffer(&m_lvl1_id, sizeof(uint32_t)));
      buffers.push_back(boost::asio::buffer(m_data.get(), m_data_size));
    }

    uint64_t AssignMessage::global_id() const
    {
      return m_global_id;
    }

    uint32_t AssignMessage::lvl1_id() const
    {
      return m_lvl1_id;
    }

    BuildMessage::BuildMessage(size_t size)
      : AssignMessage(size)
    {
    }

    uint32_t BuildMessage::typeId() const
    {
      return ID;
    }
} 

