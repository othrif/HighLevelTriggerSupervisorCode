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
    uint32_t              m_reqRoIs;
    std::vector<uint32_t> m_l1ids;
    
  };

  class AssignMessage : public daq::asyncmsg::InputMessage {
  public:

    static const uint32_t ID = 0x00DCDF01;
    explicit AssignMessage(size_t size);
    ~AssignMessage();
    virtual uint32_t typeId() const override;
    virtual uint32_t transactionId() const override;
    virtual void     toBuffers(std::vector<boost::asio::mutable_buffer>&) override;

    uint64_t         global_id() const;
    uint32_t         lvl1_id() const;

  private:
    uint64_t                    m_global_id;
    uint32_t                    m_lvl1_id;
    size_t                      m_data_size;
    std::unique_ptr<uint32_t[]> m_data;
  };
}

#endif // DCMMESSAGES_H_
