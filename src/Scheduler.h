// this is -*- c++ -*-
#ifndef HLTSV_SCHEDULER_H_
#define HLTSV_SCHEDULER_H_

#include "msg/Types.h"
#include <tr1/unordered_map>
#include "boost/thread/mutex.hpp"

namespace hltsv {

    class Node;
    class Event;

    ////
    /// Allocates a new node to be used for event processing.
    /// 

    class Scheduler {
    public:
        ~Scheduler();

        Node *select_node(Event *);

        void reset();
        void add_node(Node *);

        void disable_node(Node *);
        void enable_node(Node *);
        
        void disable_node(MessagePassing::NodeID );
        void enable_node(MessagePassing::NodeID );

        Node *find_node(MessagePassing::NodeID ) const;

    private:
        typedef std::tr1::unordered_map<MessagePassing::NodeID,Node*> Map;
        Map           m_nodes;
        Map::iterator m_next;
        boost::mutex  m_mutex;
    };

}

#endif // HLTSV_SCHEDULER_H_
