#ifndef NODEMANAGER_H
#define NODEMANAGER_H

#include <google/cloud/compute/v1/compute_engine.pb.h>
#include <aws/ec2/model/TerminateInstancesResponse.h>
#include <google/cloud/compute/v1/instances_client.h>
#include <aws/ec2/model/TerminateInstancesRequest.h>
#include "DistributedLinkedHashMap.h" // DistributedLinkedHashMap
#include <seastar/core/reactor.hh>
#include <seastar/core/seastar.hh>
#include <seastar/core/memory.hh>
#include <seastar/core/vector.hh>
#include <seastar/core/future.hh>
#include <seastar/core/print.hh>
#include <seastar/core/sleep.hh>
#include <aws/ec2/EC2Client.h>
#include <azure/identity.hpp>
#include <azure/compute.hpp>
#import "MPIController.h" // MPIController
#include <azure/core.hpp>
#include <aws/core/Aws.h>
#include "NodeQueue.hh" // NodeQueue - in-memory, thread safe Double Edge Queue, insert/delete = O(1)
#include <curl/curl.h>
#include <json/json.h>
#include <string>



#include <algorithm>
#include <iostream>
#include <vector>


class NodeManager {
public:
    NodeManager();

    seastar::future<> monitorNodes();
    seastar::future<> scaleUp(const std::string &processName);
    seastar::future<bool> needsTermination();
    
    seastar::future<> registerNode(const std::string &ipAddress, const std::string &nodeName);
    seastar::future<> unregisterNode(const std::string &ipAddress);
    seastar::future<> loadBalancer();
    seastar::future<> scaleDown();

private:
    // Private member variables for managing node state, etc.
    // You can add any necessary private methods and variables here.

    float cpuUsage;
    float totalMemory;
    float freeMemory;
    float usedMemory;
    float hddUsage;
    float trafficIn;
    float trafficOut;
    std::unordered_map<std::string, float> gpuMetrics;

    // Add threshold values for scaling decisions
    const float CPU_USAGE_THRESHOLD = 80.0f; // Example threshold
    const float MEMORY_USAGE_THRESHOLD = 75.0f; // Example threshold
    const float HDD_USAGE_THRESHOLD = 70.0f; // Example threshold
    const float GPU_USAGE_THRESHOLD = 80.0f; // Example threshold
    const float GPU_TEMPERATURE_THRESHOLD = 85.0f; // Example threshold
    const float GPU_MEMORY_USAGE_THRESHOLD = 80.0f; // Example threshold
    const float GPU_POWER_USAGE_THRESHOLD = 90.0f; // Example threshold
    const float GPU_FAN_SPEED_THRESHOLD = 90.0f; // Example threshold

    DistributedLinkedHashMap<std::string, std::string> registeredNodes;
    NodeQueue queueToScaleUp;
    NodeQueue queueToScaleDown;
    MPIController mpiController;

    seastar::future<int> getNodeLoad(const std::string &ipAddress); // Example load calculation method
    seastar::future<int> getProcessLoad(const std::string &processName); // Load calculation for processes
    seastar::future<bool> needsScaling(const std::string &processName); // Determine if a process needs scaling
    seastar::future<> gracefulShutdown(const std::string &processName); // Graceful shutdown of a process
    seastar::future<> updateProcessInfo(const std::string &oldProcessName, const std::string &newProcessName, const std::string &newIpAddress); // Update process information

    seastar::future<> scaleUpAWS(const std::string &processName);
    seastar::future<> scaleUpPaperspace(const std::string &processName);
    seastar::future<> scaleUpNebius(const std::string &processName);
    seastar::future<> scaleUpAzure(const std::string &processName);
    seastar::future<> scaleUpGCP(const std::string &processName);

    seastar::future<> deleteNodeAWS(const std::string &instanceId);
    seastar::future<> deleteNodePaperspace(const std::string &instanceId);
    seastar::future<> deleteNodeNebius(const std::string &instanceId);
    seastar::future<> deleteNodeAzure(const std::string &instanceId);
    seastar::future<> deleteNodeGCP(const std::string &instanceId);
    
    seastar::future<> process_node(NodeQueue& queue)

};

#endif // NODEMANAGER_H
