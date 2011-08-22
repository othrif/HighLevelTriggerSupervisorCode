
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
          m_port(port)
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
            m_events.push_back(event);
            return true;
        }
        return false;
    }

    void Node::free(Event *event)
    {
        boost::mutex::scoped_lock lock(m_mutex);
        EventList::iterator it = std::find(m_events.begin(), m_events.end(), event);
        if(it != m_events.end()) {
            m_events.erase(it);
        } else {
            // fatal
            ERS_LOG("Event not in Node's list" );
        }
        m_slots++;
        ERS_ASSERT_MSG(m_slots <= m_max_slots, "More calls to free() than allocate()")
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
        m_events.clear();
        m_enabled = true;
        if(slots == 0) {
            m_slots   = m_max_slots;
        } else {
            m_slots = slots;
        }
    }

    const Node::EventList& Node::events() const
    {
        return m_events;
    }
}

