
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
    ERS_LOG("tobuffer called, "<< m_prefix.reqRoIs);
    buffers.push_back(boost::asio::buffer(&m_prefix, sizeof(m_prefix)));
    buffers.push_back(boost::asio::buffer(m_l1ids));
  }
   
} 
