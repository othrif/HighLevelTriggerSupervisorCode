// this is -*- c++ -*-
#ifndef HLTSV_MESSAGES_H_
#define HLTSV_MESSAGES_H_

#include "asyncmsg/Message.h"
#include <memory>

namespace hltsv {

    class LVL1Result;
    
    /**
     * The update message from the DCM containing:
     *
     * - a list of LVL1 IDs have are rejected or built (maybe empty).
     * - an update of how many additional cores are available on this node (maybe 0).
     */
    class UpdateMessage : public daq::asyncmsg::InputMessage {
    public:

        static const uint32_t ID = 0x1000;

        explicit UpdateMessage(size_t size);
        ~UpdateMessage();
        virtual uint32_t typeId() const override;
        virtual uint32_t transactionId() const override;
        virtual void     toBuffers(std::vector<boost::asio::mutable_buffer>&) override;
    private:
        std::unique_ptr<uint32_t[]> m_data;
        uint32_t                    m_size;
    };

    /**
     * The process event message to the DCM containing:
     *
     * - the list of ROIs
     */
    class ProcessMessage : public daq::asyncmsg::OutputMessage {
    public:

        static const uint32_t ID = 0x1001;

        explicit ProcessMessage(std::shared_ptr<LVL1Result> rois);
        ~ProcessMessage();
        virtual uint32_t typeId() const override;
        virtual uint32_t transactionId() const override;
        virtual void     toBuffers(std::vector<boost::asio::const_buffer>&) const override;        
    private:
        std::shared_ptr<LVL1Result> m_rois;
    };

    /**
     * The build event message to the DCM containing:
     *
     * - the list of ROIs
     *
     * Same as ProcessMessage, just a different typeId()
     */
    class BuildMessage : public ProcessMessage {
    public:

        static const uint32_t ID = 0x1002;

        explicit BuildMessage(std::shared_ptr<LVL1Result> rois);
        virtual uint32_t typeId() const override;
    };

    /**
     * The clear message to the ROS containing:
     *
     * - a sequence number (4 bytes)
     * - the number of LVL1 IDs to follow  (4 bytes)
     * - a list of LVL1 IDs (4 bytes each)
     *
     * Note that the underlying event list may be shared by more than one clear message
     * e.g. if TCP is used instead of multicast.
     */

    class ClearMessage : public daq::asyncmsg::OutputMessage {
    public:

        static const uint32_t ID = 0x1003;

        ClearMessage(uint32_t sequence, std::shared_ptr<std::vector<uint32_t>> events);
        ~ClearMessage();
        virtual uint32_t typeId()        const override;
        virtual uint32_t transactionId() const override;
        virtual void     toBuffers(std::vector<boost::asio::const_buffer>&) const override;                
    private:
        uint32_t                               m_sequence_count[2];
        std::shared_ptr<std::vector<uint32_t>> m_events;
    };

}

#endif // HLTSV_MESSAGES_H_
