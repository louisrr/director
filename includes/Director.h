#ifndef DIRECTOR_H
#define DIRECTOR_H

#include "NodeManager.h"
#include "GANCodirector.h"
#include "MPIController.h"
#include "SystemResources.h"
#include <seastar/core/future.hh>
#include <string>
#include <zookeeper/zookeeper.h>

class Director {
public:
    Director();
    seastar::future<> nodeController();
    seastar::future<> initialize();

private:
    NodeManager nodeManager;
    GANCodirector ganCodirector;
    MPIController mpiController;
    SystemResources systemResources;
    std::string ipAddress;
    bool tbbAvailable;
    bool cudaAvailable;
    bool isLeader;

    // Zookeeper handle
    zhandle_t* zkHandle;

    seastar::future<> monitorNodes();
    void onZookeeperWatch(int type, int state, const char* path);
    void checkLeadership();
    static void watcher(zhandle_t* zh, int type, int state, const char* path, void* watcherCtx);
};

#endif // DIRECTOR_H
