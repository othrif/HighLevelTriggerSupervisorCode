// this is -*- c++ -*-
#ifndef HLTSV_CLEARMESSAGE_H_
#define HLTSV_CLEARMESSAGE_H_

#include "asyncmsg/Message.h"
#include <memory>

namespace hltsv {

    class ClearMessage : public daq::asyncmsg::OutputMessage {
    public:

        static const uint32_t ID = 0x00DCDF10;

        ClearMessage(uint32_t sequence, std::shared_ptr<std::vector<uint32_t>> events);
        ~ClearMessage();
        virtual uint32_t typeId()        const override;
        virtual uint32_t transactionId() const override;
        virtual void     toBuffers(std::vector<boost::asio::const_buffer>&) const override;                
    private:
        uint32_t                               m_sequence;
        std::shared_ptr<std::vector<uint32_t>> m_events;
    };

}

#endif // HLTSV_CLEARMESSAGE_H_
