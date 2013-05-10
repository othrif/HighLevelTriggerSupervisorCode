// this is -*- c++ -*-
#ifndef HLTSV_UPDATEMESSAGES_H_
#define HLTSV_UPDATEMESSAGES_H_

#include "asyncmsg/Message.h"
#include <memory>

namespace hltsv {

    /**
     * \brief The update message from the DCM.
     *
     * - a list of LVL1 IDs have are rejected or built (maybe empty).
     * - an update of how many additional cores are available on this node (maybe 0).
     */
    class UpdateMessage : public daq::asyncmsg::InputMessage {
    public:

      static const uint32_t ID = 0x00DCDF00;

        explicit UpdateMessage(size_t size);
        ~UpdateMessage();
        virtual uint32_t typeId() const override;
        virtual uint32_t transactionId() const override;
        virtual void     toBuffers(std::vector<boost::asio::mutable_buffer>&) override;
        uint32_t         num_request() const; // method to decode the first word of the payload
        size_t           num_l1ids() const;
        uint32_t         l1id(size_t index) const;
    private:
        std::vector<uint32_t> m_data;
    };

}

#endif // HLTSV_UPDATEMESSAGES_H_
