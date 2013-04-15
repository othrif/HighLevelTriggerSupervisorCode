// this is -*- c++ -*-
#ifndef DCMMESSAGES_H_
#define DCMMESSAGES_H_

#include "asyncmsg/Message.h"
#include <memory>



namespace hltsv {


// The request message to the HLTSV
  class RequestMessage : public daq::asyncmsg::OutputMessage {
  public:
    
    static const uint32_t ID = 0x00DCDF00;
    
    explicit RequestMessage(uint32_t reqRoIs, const std::vector<uint32_t>& l1ids);
    ~RequestMessage();
    virtual uint32_t typeId() const override;
    virtual uint32_t transactionId() const override;
    virtual void     toBuffers(std::vector<boost::asio::const_buffer>& buffers) const override;
  private:
    struct {
      uint32_t reqRoIs;
      uint32_t numl1ids;
    } m_prefix;
    std::vector<uint32_t> m_l1ids;
    
  };
}

#endif // DCMMESSAGES_H_
