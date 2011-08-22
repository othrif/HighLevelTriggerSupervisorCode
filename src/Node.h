// this is -*- c++ -*-
#ifndef HLTSV_NODE_H_
#define HLTSV_NODE_H_

#include "msg/Types.h"
#include "boost/thread/mutex.hpp"

#include <list>

namespace MessagePassing {
    class Port;
}

namespace hltsv {

    class Event;

    class Node {
    public:
        Node(MessagePassing::Port *port, unsigned int slots);
        ~Node();

        bool allocate(Event *);
        void free(Event *);

        MessagePassing::NodeID id() const;
        MessagePassing::Port   *port() const;

        bool enabled() const;

        void enable();
        void disable();

        void reset(unsigned int slots = 0);

        typedef std::list<Event*> EventList;

        const EventList& events() const;

    private:
        bool                 m_enabled;
        unsigned int         m_slots;
        unsigned int         m_max_slots;
        MessagePassing::Port *m_port;
        EventList            m_events;
        boost::mutex         m_mutex;
    };
}

#endif // HLTSV_NODE_H_
