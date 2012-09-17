
#include "ers/Assertion.h"
#include "msg/Port.h"

#include "Node.h"
#include "Event.h"

#include <algorithm>

namespace hltsv {

    Node::Node(MessagePassing::Port *port, unsigned int slots)
        : m_enabled(true),
          m_slots(slots),
          m_max_slots(slots),
          m_port(port),
          m_events(slots,0)
    {
        ERS_PRECONDITION(port != 0);
        ERS_PRECONDITION(slots > 0);
    }

    Node::~Node()
    {
    }

    bool Node::allocate(Event *event)
    {
        boost::mutex::scoped_lock lock(m_mutex);
        if(m_enabled && m_slots > 0) {
            m_slots--;
            event->handled_by(this);
            EventList::iterator it = std::find(m_events.begin(), m_events.end(), (Event *)0);
            ERS_ASSERT_MSG(it != m_events.end(), "No free slot in event list");
            *it = event;
            return true;
        }
        return false;
    }

    void Node::free(Event *event)
    {
        boost::mutex::scoped_lock lock(m_mutex);
        EventList::iterator it = std::find(m_events.begin(), m_events.end(), event);
        ERS_ASSERT_MSG(it != m_events.end(), "Event not in Node's list");
        *it = 0;
    }

    MessagePassing::NodeID Node::id() const
    {
        return m_port->id();
    }

    MessagePassing::Port   *Node::port() const
    {
        return m_port;
    }

    bool Node::enabled() const
    {
        return m_enabled;
    }

    void Node::enable()
    {
        m_enabled = true;
    }

    void Node::disable()
    {
        m_enabled = false;
    }


    void Node::reset(unsigned int slots)
    {
        boost::mutex::scoped_lock lock(m_mutex);
        if(slots == 0) {
            m_slots   = m_max_slots;
        } else {
            m_max_slots = m_slots = slots;
        }
        m_events.clear();
        m_events.resize(m_slots, 0);
        m_enabled = true;
    }

    void Node::update(unsigned int slots)
    {
        boost::mutex::scoped_lock lock(m_mutex);
        m_slots += slots;
        ERS_ASSERT_MSG(m_slots <= m_max_slots, "Too many slots added to node " << id());
    }

    const Node::EventList& Node::events() const
    {
        return m_events;
    }
}

