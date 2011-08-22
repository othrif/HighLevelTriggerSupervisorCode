// this is -*- c++ -*-
#ifndef HLTSV_NODESET_H_
#define HLTSV_NODESET_H_

#include "msg/Types.h"
#include <list>
#include <tr1/unordered_map>
#include "boost/thread/mutex.hpp"

namespace hltsv {

    class Node;

    /**
     * Manages the global list of nodes.
     *
     * One or more Scheduler objects can be instantiated
     * with a reference to this object and use it concurrently.
     */
    class NodeSet {
    public:
        ~NodeSet();

        void reset();
        void add_node(Node *);

        void disable_node(Node *);
        void enable_node(Node *);
        
        void disable_node(MessagePassing::NodeID );
        void enable_node(MessagePassing::NodeID );

        Node *find_node(MessagePassing::NodeID ) const;

        // Scheduler's access the nodes via a list iterator
        // which does not become invalid after insertion of
        // new nodes...
        typedef std::list<Node*> List;
        typedef List::iterator iterator;

        iterator begin();
        iterator end();
        size_t   size();

    private:
        typedef std::tr1::unordered_map<uint32_t, Node*> Map;
        List          m_nodes;
        Map           m_indexed_nodes;
        mutable boost::mutex  m_mutex;
    };

}

#endif // HLTSV_NODESET_H_
