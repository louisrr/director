#ifndef DYNAMIC_RESOURCE_MANAGER_H
#define DYNAMIC_RESOURCE_MANAGER_H

#include <vector>
#include <string>
#include <map>
#include <tuple>
#include <nvml.h>
#include <stdexcept>

class DynamicResourceManager {
public:
    DynamicResourceManager();
    ~DynamicResourceManager();

    double gpuUsage();
    double gpuTemperature(int instanceId);
    double cpuUsage(int instanceId);
    double memoryUsage(int instanceId);
    double driveUsage(int instanceId);
    std::vector<std::string> findBottleNecks();
    std::map<int, std::string> taskManager();
    void taskReaper(int taskId);
    void instanceTracker();

private:
    void healthReporter(int instanceId);

    // Private member variables to store resource information
    std::map<int, double> gpuUsageMap;
    std::map<int, double> gpuTemperatureMap;
    std::map<int, double> cpuUsageMap;
    std::map<int, double> memoryUsageMap;
    std::map<int, double> driveUsageMap;
    std::map<int, std::string> taskStatusMap;
    void initNVML();
    void shutdownNVML();
};

#endif // DYNAMIC_RESOURCE_MANAGER_H
