// this is -*- c++ -*-
#ifndef HLTSV_PROCESSMESSAGE_H_
#define HLTSV_PROCESSMESSAGE_H_

#include "asyncmsg/Message.h"
#include <memory>

namespace hltsv {

    class LVL1Result;
    
    /**
     * \brief The process event message to the DCM.
     *
     * - the global event ID.
     * - the LVL1 ID.
     * - the list of ROIs.
     */
    class ProcessMessage : public daq::asyncmsg::OutputMessage {
    public:

      static const uint32_t ID = 0x00DCDF01;

        explicit ProcessMessage(std::shared_ptr<LVL1Result> rois);
        ~ProcessMessage();
        virtual uint32_t typeId() const override;
        virtual uint32_t transactionId() const override;
        virtual void     toBuffers(std::vector<boost::asio::const_buffer>&) const override;
    private:
        std::shared_ptr<LVL1Result> m_rois;
    };

    /**
     * \brief The build event message to the DCM.
     *
     * - the global event ID.
     * - the LVL1 ID.
     * - the list of ROIs.
     *
     * Same as ProcessMessage, just a different typeId()
     */
    class BuildMessage : public ProcessMessage {
    public:

        static const uint32_t ID = 0x00DCDF0F;

        explicit BuildMessage(std::shared_ptr<LVL1Result> rois);
        virtual uint32_t typeId() const override;
    };

}

#endif // HLTSV_PROCESSMESSAGE_H_
