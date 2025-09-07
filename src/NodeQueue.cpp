#include "NodeQueue.h"

// Define the NodeQueue class using std::deque and Seastar primitives
NodeQueue::NodeQueue() : _sem(1) {}  // Initialize semaphore with a count of 1

// Enqueue a node at the front
seastar::future<> NodeQueue::enqueue_front(Node node) {
    return _sem.wait().then([this, node = std::move(node)] {
        _queue.push_front(node);
        _sem.signal();
    });
}

// Enqueue a node at the back
seastar::future<> NodeQueue::enqueue_back(Node node) {
    return _sem.wait().then([this, node = std::move(node)] {
        _queue.push_back(node);
        _sem.signal();
    });
}

// Dequeue a node from the front
seastar::future<Node> NodeQueue::dequeue_front() {
    return _sem.wait().then([this] {
        if (_queue.empty()) {
            _sem.signal();
            return seastar::make_ready_future<Node>(Node{-1});  // Return a special value for empty queue
        } else {
            Node node = _queue.front();
            _queue.pop_front();
            _sem.signal();
            return seastar::make_ready_future<Node>(node);
        }
    });
}

// Dequeue a node from the back
seastar::future<Node> NodeQueue::dequeue_back() {
    return _sem.wait().then([this] {
        if (_queue.empty()) {
            _sem.signal();
            return seastar::make_ready_future<Node>(Node{-1});  // Return a special value for empty queue
        } else {
            Node node = _queue.back();
            _queue.pop_back();
            _sem.signal();
            return seastar::make_ready_future<Node>(node);
        }
    });
}

// Check if the queue is empty
seastar::future<bool> NodeQueue::is_empty() {
    return _sem.wait().then([this] {
        bool empty = _queue.empty();
        _sem.signal();
        return seastar::make_ready_future<bool>(empty);
    });
}

// Function to process nodes asynchronously
seastar::future<> process_node(NodeQueue& queue) {
    return seastar::repeat([&queue]() {
        return queue.dequeue_front().then([](Node node) {
            if (node.id == -1) {
                return seastar::stop_iteration::yes;  // Stop if the queue is empty
            }
            // Process the node (scale up, scale down, or delete)
            std::cout << "Processing Node " << node.id << "\n";
            return seastar::stop_iteration::no;
        });
    });
}

// Function to add nodes to the queue
seastar::future<> add_nodes(NodeQueue& queue) {
    return seastar::do_for_each(boost::irange(1, 10), [&queue](int i) {
        Node node{i};
        return queue.enqueue_back(node);
    });
}