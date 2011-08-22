// this is -*- c++ -*-
#ifndef HLTSV_SCHEDULER_H_
#define HLTSV_SCHEDULER_H_

#include "NodeSet.h"

namespace hltsv {

    class Node;
    class Event;

    ////
    /// Allocates a new node to be used for event processing.
    /// 

    class Scheduler {
    public:
        Scheduler(NodeSet& nodes, unsigned int offset);
        Node *select_node(Event *);

    private:
        NodeSet&          m_nodes;
        NodeSet::iterator m_next;
    };

}

#endif // HLTSV_SCHEDULER_H_
