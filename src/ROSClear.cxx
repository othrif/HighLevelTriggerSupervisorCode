
#include "ROSClear.h"
#include "Messages.h"
#include "asyncmsg/NameService.h"

namespace hltsv {

    ROSClear::ROSClear(size_t threshold)
        : m_threshold(threshold)
    {
    }

    ROSClear::~ROSClear()
    {
        // ERS_ASSERT(m_event_ids.empty());

        // We cannot call flush() here since the
        // derived object no longer exists at this point.
    }
        
    // Add event to group of processed events. If threshold
    // reached, send ROS clear message to ROS.
    void ROSClear::add_event(uint32_t l1_event_id)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_event_ids.push_back(l1_event_id);

        if(m_event_ids.size() >= m_threshold) {
            std::shared_ptr<std::vector<uint32_t>> data(new std::vector<uint32_t>);
            m_event_ids.swap(*data);
            uint32_t seq = m_sequence++;
            lock.unlock();
            do_flush(seq, data);
        }
    }

    void ROSClear::flush()
    {
        std::shared_ptr<std::vector<uint32_t>> data(new std::vector<uint32_t>);
        uint32_t seq;

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_event_ids.swap(*data);
            seq = m_sequence++;
        }
        do_flush(seq, data);
    }

    UnicastROSClear::UnicastROSClear(size_t threshold, 
                                     boost::asio::io_service& service, 
                                     daq::asyncmsg::NameService& name_service)
        : ROSClear(threshold)
    {
        std::vector<boost::asio::ip::tcp::endpoint> ros_eps = name_service.lookup("CLEAR");

        // ERS_ASSERT(ros_eps != 0)  ? or just warning ?

        for(size_t i = 0; i < ros_eps.size(); i++) {
            m_sessions.push_back(std::make_shared<ROSSession>(service));
        }

        for(size_t i = 0; i < ros_eps.size(); i++) {
            m_sessions[i]->asyncOpen("HLTSV",ros_eps[i]);
        }
        
    }

    UnicastROSClear::~UnicastROSClear()
    {
        // must do this here, while object still exists.
        flush();
    }

    void UnicastROSClear::do_flush(uint32_t sequence, std::shared_ptr<std::vector<uint32_t>> events)
    {
        for(auto session : m_sessions) {
            std::unique_ptr<const daq::asyncmsg::OutputMessage> msg(new ClearMessage(sequence,events));
            session->asyncSend(std::move(msg));
        }
    }

}
