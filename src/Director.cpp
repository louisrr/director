#include "Director.h"
#include <iostream>
#include <chrono>
#include <ctime>

Director::Director()
    : ganCodirector(), mpiController(), systemResources() {
    ipAddress = SystemResources::getIpAddress();
    tbbAvailable = SystemResources::isTbbAvailable();
    cudaAvailable = SystemResources::hasNvidiaGpu();
    isLeader = false;

    zkHandle = zookeeper_init("localhost:2181", watcher, 2000, 0, this, 0);
    if (!zkHandle) {
        std::cerr << "Error connecting to Zookeeper server!" << std::endl;
        exit(EXIT_FAILURE);
    }
}

seastar::future<> Director::initialize() {
    return seastar::async([this] {
        checkLeadership();
    });
}

seastar::future<> Director::nodeController() {
    if (isLeader) {
        return monitorNodes().then([this] {
            ganCodirector.train("path/to/dataset");
            ganCodirector.generate("path/to/output");
            return seastar::make_ready_future<>();
        });
    } else {
        return seastar::make_ready_future<>();
    }
}

seastar::future<> Director::monitorNodes() {
    return seastar::repeat([this] {
        return nodeManager.monitorNodes().then([] {
            seastar::print("Monitoring nodes completed.\n");
            return seastar::sleep(std::chrono::seconds(10)); // Adjust the interval as needed
        }).handle_exception([](std::exception_ptr ex) {
            seastar::print("Error during monitoring: %s\n", seastar::current_exception_as_string().c_str());
            return seastar::sleep(std::chrono::seconds(10)); // Adjust the interval as needed
        }).then([] {
            return seastar::stop_iteration::no;
        });
    });
}

void Director::checkLeadership() {
    int rc = zoo_create(zkHandle, "/director/leader", ipAddress.c_str(), ipAddress.size(),
                        &ZOO_OPEN_ACL_UNSAFE, ZOO_EPHEMERAL, nullptr, 0);
    if (rc == ZOK) {
        std::cout << "I am the leader" << std::endl;
        isLeader = true;
        nodeController();
    } else {
        std::cout << "I am a follower" << std::endl;
        isLeader = false;
        rc = zoo_wexists(zkHandle, "/director/leader", watcher, this, nullptr);
    }
}

void Director::onZookeeperWatch(int type, int state, const char* path) {
    if (type == ZOO_DELETED_EVENT) {
        checkLeadership();
    }
}

void Director::watcher(zhandle_t* zh, int type, int state, const char* path, void* watcherCtx) {
    Director* director = static_cast<Director*>(watcherCtx);
    director->onZookeeperWatch(type, state, path);
}
