#include "DynamicResourceManager.h"
#include <iostream>
#include <mpi.h>
#include <algorithm>
#include <chrono>
#include <thread>

DynamicResourceManager::DynamicResourceManager() {
    initNVML();
}

DynamicResourceManager::~DynamicResourceManager() {
    shutdownNVML();
}

double DynamicResourceManager::gpuUsage() {
    // Aggregate GPU usage from all instances
    double totalGpuUsage = 0.0;
    for (const auto& [id, usage] : gpuUsageMap) {
        totalGpuUsage += usage;
    }
    return totalGpuUsage;
}

void DynamicResourceManager::initNVML() {
    nvmlReturn_t result = nvmlInit();
    if (NVML_SUCCESS != result) {
        throw std::runtime_error("Failed to initialize NVML: " + std::string(nvmlErrorString(result)));
    }
}

void DynamicResourceManager::shutdownNVML() {
    nvmlReturn_t result = nvmlShutdown();
    if (NVML_SUCCESS != result) {
        std::cerr << "Failed to shutdown NVML: " + std::string(nvmlErrorString(result)) << std::endl;
    }
}

double DynamicResourceManager::gpuTemperature(int instanceId) {
    nvmlDevice_t device;
    nvmlReturn_t result = nvmlDeviceGetHandleByIndex(deviceIndex, &device);
    if (NVML_SUCCESS != result) {
        throw std::runtime_error("Failed to get handle for device " + std::to_string(deviceIndex) + ": " + std::string(nvmlErrorString(result)));
    }

    unsigned int temperature;
    result = nvmlDeviceGetTemperature(device, NVML_TEMPERATURE_GPU, &temperature);
    if (NVML_SUCCESS != result) {
        throw std::runtime_error("Failed to get temperature for device " + std::to_string(deviceIndex) + ": " + std::string(nvmlErrorString(result)));
    }

    return static_cast<double>(temperature);
}

double DynamicResourceManager::cpuUsage(int instanceId) {
    // Return CPU usage for the given instance
    if (cpuUsageMap.find(instanceId) != cpuUsageMap.end()) {
        return cpuUsageMap[instanceId];
    }
    return 0.0; // or an appropriate error value
}

double DynamicResourceManager::memoryUsage(int instanceId) {
    // Return memory usage for the given instance
    if (memoryUsageMap.find(instanceId) != memoryUsageMap.end()) {
        return memoryUsageMap[instanceId];
    }
    return 0.0; // or an appropriate error value
}

double DynamicResourceManager::driveUsage(int instanceId) {
    // Return drive usage for the given instance
    if (driveUsageMap.find(instanceId) != driveUsageMap.end()) {
        return driveUsageMap[instanceId];
    }
    return 0.0; // or an appropriate error value
}

std::vector<std::string> DynamicResourceManager::findBottleNecks() {
    // Identify bottlenecks based on resource usage
    std::vector<std::string> bottlenecks;
    // Example logic to find bottlenecks
    for (const auto& [id, usage] : cpuUsageMap) {
        if (usage > 80.0) { // Assuming 80% usage is a bottleneck
            bottlenecks.push_back("CPU usage high on instance " + std::to_string(id));
        }
    }
    // Similar checks for other resources
    return bottlenecks;
}

std::map<int, std::string> DynamicResourceManager::taskManager() {
    // Return a map of task IDs and their statuses
    return taskStatusMap;
}

void DynamicResourceManager::taskReaper(int taskId) {
    // Stop the task with the given ID
    taskStatusMap.erase(taskId);
    std::cout << "Task " << taskId << " has been stopped." << std::endl;
}

void DynamicResourceManager::instanceTracker() {
    // Periodically poll instances for their statuses
    while (true) {
        for (const auto& [id, _] : gpuUsageMap) {
            healthReporter(id);
        }
        std::this_thread::sleep_for(std::chrono::seconds(60)); // Poll every 60 seconds
    }
}

void DynamicResourceManager::healthReporter(int instanceId) {
    // Simulate gathering resource usage data via MPI
    MPI_Init(NULL, NULL);
    int world_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    // Replace the following with actual MPI communication logic
    gpuUsageMap[instanceId] = 30.0 + world_rank; // Simulated value
    gpuTemperatureMap[instanceId] = 70.0 + world_rank; // Simulated value
    cpuUsageMap[instanceId] = 50.0 + world_rank; // Simulated value
    memoryUsageMap[instanceId] = 60.0 + world_rank; // Simulated value
    driveUsageMap[instanceId] = 40.0 + world_rank; // Simulated value

    MPI_Finalize();
}
