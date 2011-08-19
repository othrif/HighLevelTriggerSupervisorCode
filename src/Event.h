// this is -*- c++ -*-
#ifndef HLTSV_EVENT_H_
#define HLTSV_EVENT_H_

#include "clocks/Time.h"

namespace dcmessages {
    class LVL1Result;
}


namespace hltsv {

    class Node;

    /**
     * Keeps all information related to an active event while
     * it is being processed.
     */
    class Event {
    public:
        Event(dcmessages::LVL1Result *l1);
        ~Event();

        unsigned int lvl1_id() const;

        void handled_by(Node *);
        Node *handled_by() const;

        // Set the done flags
        void done();
        
        // still processing ?
        bool active() const;

        // time when event was assigned
        Time assigned() const;

        dcmessages::LVL1Result *l1result() const;

    private:

        dcmessages::LVL1Result   *m_l1;
        bool                     m_done;
        unsigned int             m_lvl1_id;
        Node                     *m_node;
        daq::clocks::Time        m_assigned;
    };
}

#endif // HLTSV_NODE_H_
