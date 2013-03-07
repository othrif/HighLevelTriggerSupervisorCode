// this is -*- C++ -*-
#ifndef HLTSV_ROSCLEAR_H_
#define HLTSV_ROSCLEAR_H_

#include <cstdint>
#include <mutex>
#include <vector>
#include <string>
#include <memory>

namespace hltsv {

    //
    // The ROSClear interface.
    // This is all the rest of the HLTSV sees.
    //
    // The instances of ROSClear should not be
    // created before the remote side is ready, i.e.
    // not before the connect() transition.
    //
    class ROSClear {
    public:
        
        /**
         * The threshold is the number of events after which
         * the internal list of event IDs is sent to the ROS.
         */
        ROSClear(size_t threshold = 100);
        virtual ~ROSClear();

        ROSClear(const ROSClear& ) = delete;
        ROSClear& operator=(const ROSClear& ) = delete;

        /** 
         * Add event to group of processed events. If threshold
         * reached, send ROS clear message to ROS.
         */
        void add_event(uint32_t l1_event_id);

        // Flush all events to the ROS.
        void flush();

    protected:
        
        /** 
         * Send any existing events to the ROS.
         * This is implemented in a derived class, using either
         * multicast or multiple sends.
         */
        virtual void do_flush(std::shared_ptr<std::vector<uint32_t>> events) = 0;

    private:
        size_t                m_threshold;
        std::mutex            m_mutex;
        std::vector<uint32_t> m_event_ids;
    };

    //
    // Two possible implementations. These can go into a separate
    // file if necessary.
    //
    // Who creates the ROSClear instance ? 
    // Either via a ROSClear::create() function (arguments ??)
    // or explicitly by the Activity::configure() method ? 
    // 

    class MultiCastROSClear : public ROSClear {
    public:
        MultiCastROSClear(size_t threshold, const std::string& address_network);
        ~MultiCastROSClear();

        virtual void do_flush(std::shared_ptr<std::vector<uint32_t>> events) override;
    private:
        // TODO:
        // 
        // - initialize the multicast socket with the given
        //   address and outgoing network interface.
        // 
        // do_flush(data):
        //
        //   async_send(m_socket, ...);
        //   TODO: what about queuing ???
        //
        // boost::asio::ip::udp::socket m_socket;
    };

    class UnicastROSClear : public ROSClear {
    public:
        UnicastROSClear(size_t threshold, const std::vector<std::string>& data_networks, const std::string& is_server = "DF");
        ~UnicastROSClear();

        virtual void do_flush(std::shared_ptr<std::vector<uint32_t>> events) override;

    private:
        // TODO:
        //
        // using msgnamesvc::NameService,
        // lookup all 'CLEAR_*' entries and connect to them. 
        // Add each session to m_sessions.
        //
        // do_flush(data):
        //        //
        //   for(each session) {
        //      uniq_ptr<ClearMessage> msg(new ClearMessage(sequence, data));
        //      session->send(msg);
        // 
        // std::vector<std::shared_ptr<std::ROSSession>> m_sessions;
    };

    
}

#endif // HLTSV_ROSCLEAR_H_
