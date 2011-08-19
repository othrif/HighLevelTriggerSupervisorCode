
#include "Scheduler.h"
#include "Node.h"

namespace hltsv {

    Scheduler::~Scheduler()
    {}

    
    Node *Scheduler::select_node(Event *event)
    {
        boost::mutex::scoped_lock lock(m_mutex);
        Map::iterator it = m_next;

        while(true) {
            if(it == m_nodes.end()) {
                it = m_nodes.begin();
            }
            if(it->second->allocate(event)) {
                Node *result = it->second;
                ++it;
                m_next = it;
                return result;
            }
            ++it;
        }
    }

    void Scheduler::reset()
    {
        boost::mutex::scoped_lock lock(m_mutex);
        for(Map::iterator it = m_nodes.begin();
            it != m_nodes.end();
            ++it) {
            it->second->reset();
        }
    }

    void Scheduler::add_node(Node *node)
    {
        boost::mutex::scoped_lock lock(m_mutex);
        m_nodes[node->id()] = node;
        m_next = m_nodes.begin();
    }

    void Scheduler::disable_node(MessagePassing::NodeID id)
    {
        if(Node *node = find_node(id)) {
            node->disable();
        }
    }

    void Scheduler::enable_node(MessagePassing::NodeID id)
    {
        if(Node *node = find_node(id)) {
            node->enable();
        }
    }

    Node *Scheduler::find_node(MessagePassing::NodeID id) const
    {
        Map::const_iterator it = m_nodes.find(id);
        if(it != m_nodes.end()) {
            return it->second;
        }
        return 0;
    }

}

