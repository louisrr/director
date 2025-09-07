#ifndef DIRECTOR_H
#define DIRECTOR_H

#include "DistributedLinkedHashMap.h"
#include <seastar/core/future.hh>
#include <zookeeper/zookeeper.h>
#include "NodeManager.h"
#include "Pipeline.h"


class Director {
public:
    seastar::future<> nodeController();

private:
    NodeManager nodeManager;
    std::string ipAddress;
    bool tbbAvailable;
    bool cudaAvailable;};

#endif // DIRECTOR_H
