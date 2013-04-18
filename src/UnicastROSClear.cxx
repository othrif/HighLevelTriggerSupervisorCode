
#include "UnicastROSClear.h"
#include "Messages.h"
#include "asyncmsg/NameService.h"

namespace hltsv {

    UnicastROSClear::UnicastROSClear(size_t threshold, 
                                     boost::asio::io_service& service, 
                                     daq::asyncmsg::NameService& name_service)
        : ROSClear(threshold)
    {
        m_endpoints = name_service.lookup("CLEAR");

        // ERS_ASSERT(m_endpoints.size() != 0)  ? or just warning ?

        for(size_t i = 0; i < m_endpoints.size(); i++) {
            m_sessions.push_back(std::make_shared<ROSSession>(service));
        }
        
    }

    UnicastROSClear::~UnicastROSClear()
    {
        // must do this here, while object still exists.
        flush();
    }

    void UnicastROSClear::connect()
    {
        for(size_t i = 0; i < m_endpoints.size(); i++) {
            m_sessions[i]->asyncOpen("HLTSV", m_endpoints[i]);
        }

    }

    void UnicastROSClear::do_flush(uint32_t sequence, std::shared_ptr<std::vector<uint32_t>> events)
    {
        using namespace daq::asyncmsg;

        ERS_LOG("Flushing " << events->size() << " L1 IDs");

        for(auto session : m_sessions) {
            std::unique_ptr<const OutputMessage> msg(new ClearMessage(sequence,events));
            session->asyncSend(std::move(msg));
        }
    }

}
