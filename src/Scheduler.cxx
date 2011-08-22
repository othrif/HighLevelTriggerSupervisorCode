
#include "Scheduler.h"
#include "Node.h"

#include <iterator>

namespace hltsv {

    Scheduler::Scheduler(NodeSet& m_nodes, unsigned int offset)
        : m_nodes(m_nodes),
          m_next(m_nodes.begin())
    {
        std::advance(m_next, offset);
    }

    Node *Scheduler::select_node(Event *event)
    {
        NodeSet::iterator it = m_next;

        // this assumes that no nodes are added the
        // NodeSet while we are scheduling, only
        // disabled or enabled.

        while(true) {
            if(it == m_nodes.end()) {
                it = m_nodes.begin();
            }

            if((*it)->allocate(event)) {
                Node *result = *it;
                ++it;
                m_next = it;
                return result;
            }
            ++it;
        }
    }

}

