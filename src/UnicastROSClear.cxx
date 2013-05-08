
#include "UnicastROSClear.h"
#include "Messages.h"
#include "asyncmsg/NameService.h"

namespace hltsv {

    UnicastROSClear::UnicastROSClear(size_t threshold, 
                                     boost::asio::io_service& service, 
                                     daq::asyncmsg::NameService& name_service)
        : ROSClear(threshold),
          m_service(service),
          m_name_service(name_service)
    {
    }

    UnicastROSClear::~UnicastROSClear()
    {
        // must do this here, while object still exists.
        flush();
    }

    void UnicastROSClear::connect()
    {
        auto endpoints = m_name_service.lookup("CLEAR");

        ERS_LOG("Found " << endpoints.size() << " ROSes");

        // ERS_ASSERT(m_endpoints.size() != 0)  ? or just warning ?

        for(size_t i = 0; i < endpoints.size(); i++) {
            m_sessions.push_back(std::make_shared<ROSSession>(m_service));
        }

        for(size_t i = 0; i < endpoints.size(); i++) {
            ERS_LOG("Connection ROSSession to " << endpoints[i]);
            m_sessions[i]->asyncOpen("HLTSV", endpoints[i]);
        }

        for(auto& session : m_sessions) {
            while(session->state() != daq::asyncmsg::Session::State::OPEN) {
                usleep(10000);
            }
        }

    }

    void UnicastROSClear::do_flush(uint32_t sequence, std::shared_ptr<std::vector<uint32_t>> events)
    {
        using namespace daq::asyncmsg;

        ERS_DEBUG(1,"Flushing " << events->size() << " L1 IDs");

        for(auto session : m_sessions) {
            std::unique_ptr<const OutputMessage> msg(new ClearMessage(sequence,events));
            session->asyncSend(std::move(msg));
        }
    }

}
