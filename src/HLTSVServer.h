// -*- c++ -*-
#include "asyncmsg/Server.h"

namespace daq { 
    namespace hltsv {

        class HLTSVServer : public daq::asyncmsg::Server {
        public:
            virtual void onAccept(std::shared_ptr<Session> session) noexcept override;

            virtual void onAcceptError(const boost::system::error_code& error,
                                       std::shared_ptr<Session> session) noexcept override;
            
        };

    }
}
