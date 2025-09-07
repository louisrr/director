// node_queue.hh

#ifndef NODE_QUEUE_HH
#define NODE_QUEUE_HH

#include <seastar/core/app-template.hh>
#include <seastar/core/future.hh>
#include <seastar/core/reactor.hh>
#include <seastar/core/semaphore.hh>
#include <deque>
#include <iostream>

// Define the Node structure
struct Node {
    int id;
    // Additional fields can be added here
};

// Define the NodeQueue class using std::deque and Seastar primitives
class NodeQueue {
public:
    NodeQueue();

    // Enqueue a node at the front
    seastar::future<> enqueue_front(Node node);

    // Enqueue a node at the back
    seastar::future<> enqueue_back(Node node);

    // Dequeue a node from the front
    seastar::future<Node> dequeue_front();

    // Dequeue a node from the back
    seastar::future<Node> dequeue_back();

    // Check if the queue is empty
    seastar::future<bool> is_empty();

private:
    std::deque<Node> _queue;          // Underlying container
    seastar::semaphore _sem;          // Semaphore for synchronization
};

// Function to process nodes asynchronously
seastar::future<> process_node(NodeQueue& queue);

// Function to add nodes to the queue
seastar::future<> add_nodes(NodeQueue& queue);

#endif // NODE_QUEUE_HH
