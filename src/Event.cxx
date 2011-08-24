
#include "Event.h"
#include "Node.h"
#include "dcmessages/LVL1Result.h"
#include "dcmessages/LVL2Decision.h"
#include "clocks/Clock.h"

#include "ers/Assertion.h"

namespace hltsv {

    Event::Event(dcmessages::LVL1Result *l1)
        : m_l1(l1),
          m_done(false),
          m_node(0)
    {
        ERS_ASSERT_MSG(l1 != 0, "No LVL1Result");
    }

    Event::~Event()
    {
        delete m_l1;
    }

    unsigned int Event::lvl1_id() const
    {
        return m_l1->l1ID();
    }

    void Event::handled_by(Node *node)
    {
        m_node = node;

        static RealTimeClock clock;
        m_assigned = clock.time();
    }
    
    Node* Event::handled_by() const
    {
        return m_node;
    }
    
    void Event::done()
    {
        ERS_ASSERT_MSG(!m_done, "Event::done called twice");
        m_done = true;
        m_node->free(this);
        m_node = 0;
    }

    bool Event::active() const
    {
        return !m_done;
    }

    Time Event::assigned() const
    {
        return m_assigned;
    }

    dcmessages::LVL1Result *Event::l1result() const
    {
        return m_l1;
    }

}
