
#include "Scheduler.h"
#include "Node.h"

namespace hltsv {

    NodeSet::~NodeSet()
    {}


    void NodeSet::reset()
    {
        boost::mutex::scoped_lock lock(m_mutex);
        for(List::iterator it = m_nodes.begin();
            it != m_nodes.end();
            ++it) {
            (*it)->reset();
        }
    }

    void NodeSet::add_node(Node *node)
    {
        boost::mutex::scoped_lock lock(m_mutex);
        m_nodes.push_back(node);
        m_indexed_nodes[node->id()] = node;
    }

    void NodeSet::disable_node(MessagePassing::NodeID id)
    {
        if(Node *node = find_node(id)) {
            node->disable();
        }
    }

    void NodeSet::enable_node(MessagePassing::NodeID id)
    {
        if(Node *node = find_node(id)) {
            node->enable();
        }
    }

    Node *NodeSet::find_node(MessagePassing::NodeID id) const
    {
        boost::mutex::scoped_lock lock(m_mutex);
        Map::const_iterator it = m_indexed_nodes.find(id);
        if(it != m_indexed_nodes.end()) {
            return it->second;
        }
        return 0;
    }

    NodeSet::iterator NodeSet::begin()
    {
        return m_nodes.begin();
    }
    NodeSet::iterator NodeSet::end()
    {
        return m_nodes.end();
    }

    size_t   NodeSet::size()
    {
        return m_nodes.size();
    }

}

