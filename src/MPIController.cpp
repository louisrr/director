#include "MPIController.h"

void distributeAndGatherMemoryUsage(const std::vector<pid_t>& pids, int target_rank) {
    int world_size;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    int world_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    // Only execute on the specified target rank
    if (world_rank != target_rank) {
        return;
    }

    std::vector<pid_t> local_pids;

    // Distribute pids to each process
    int num_pids = pids.size();
    int chunk_size = num_pids / world_size;
    int start_index = world_rank * chunk_size;
    int end_index = (world_rank == world_size - 1) ? num_pids : start_index + chunk_size;

    for (int i = start_index; i < end_index; ++i) {
        local_pids.push_back(pids[i]);
    }

    // Each process calculates memory usage for its local pids
    auto memory_usage = SystemMetrics::getProcessMemoryUsage(local_pids);

    // Gather results from all processes
    std::vector<int> all_memory_usages;
    if (world_rank == 0) {
        all_memory_usages.resize(num_pids);
    }

    MPI_Gather(memory_usage.data(), memory_usage.size(), MPI_INT, 
               all_memory_usages.data(), memory_usage.size(), MPI_INT, 
               0, MPI_COMM_WORLD);

    // Process 0 can now aggregate the results
    if (world_rank == 0) {
        for (int i = 0; i < num_pids; ++i) {
            std::cout << "PID: " << pids[i] << ", Memory Usage: " << all_memory_usages[i] << std::endl;
        }
    }
}

float MPIController::mpiWrapperFunction(const std::string& target_ip, std::function<float(unsigned int)> func, unsigned int gpuIndex) {
    int world_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    // Get the IP address of the current node
    std::string current_ip = getIPAddress();

    float result = 0.0;

    if (current_ip == target_ip) {
        result = func(gpuIndex);
    }

    float global_result = 0.0;
    MPI_Reduce(&result, &global_result, 1, MPI_FLOAT, MPI_SUM, 0, MPI_COMM_WORLD);

    if (current_ip == target_ip) {
        return global_result;
    }

    return 0.0;
}

double MPIController::mpiWrapperFunction(const std::string& target_ip, std::function<double()> func) {
    int world_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    // Get the IP address of the current node
    std::string current_ip = getIPAddress();

    double result = 0.0;

    if (current_ip == target_ip) {
        result = func();
    }

    double global_result = 0.0;
    MPI_Reduce(&result, &global_result, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    if (current_ip == target_ip) {
        return global_result;
    }

    return 0.0;
}

float MPIController::mpiGetGpuTemperature(const std::string& target_ip, unsigned int gpuIndex) {
    return mpiWrapperFunction(target_ip, SystemMetrics::getGpuTemperature, gpuIndex);
}

float MPIController::mpiGetGpuUsage(const std::string& target_ip, unsigned int gpuIndex) {
    return mpiWrapperFunction(target_ip, SystemMetrics::getGpuUsage, gpuIndex);
}

float MPIController::mpiGetGpuMemoryUsage(const std::string& target_ip, unsigned int gpuIndex) {
    return mpiWrapperFunction(target_ip, SystemMetrics::getGpuMemoryUsage, gpuIndex);
}

float MPIController::mpiGetGpuPowerUsage(const std::string& target_ip, unsigned int gpuIndex) {
    return mpiWrapperFunction(target_ip, SystemMetrics::getGpuPowerUsage, gpuIndex);
}

float MPIController::mpiGetGpuFanSpeed(const std::string& target_ip, unsigned int gpuIndex) {
    return mpiWrapperFunction(target_ip, NvidiaGPUInfo::getGpuFanSpeed, gpuIndex);
}

float MPIController::mpiGetGpuCoreClock(const std::string& target_ip, unsigned int gpuIndex) {
    return mpiWrapperFunction(target_ip, SystemMetrics::getGpuCoreClock, gpuIndex);
}

float MPIController::mpiGetGpuMemoryClock(const std::string& target_ip, unsigned int gpuIndex) {
    return mpiWrapperFunction(target_ip, SystemMetrics::getGpuMemoryClock, gpuIndex);
}

double MPIController::mpiGetAvailableMemory(const std::string& target_ip) {
    return mpiWrapperFunction(target_ip, SystemMetrics::getAvailableMemory);
}

// New gpuMetrics function
seastar::future<std::unordered_map<std::string, float>> MPIController::gpuMetrics(const std::string& target_ip, unsigned int gpuIndex) {
    return seastar::async([=] {
        std::unordered_map<std::string, float> metrics;
        metrics["GpuTemperature"] = mpiGetGpuTemperature(target_ip, gpuIndex);
        metrics["GpuUsage"] = mpiGetGpuUsage(target_ip, gpuIndex);
        metrics["GpuMemoryUsage"] = mpiGetGpuMemoryUsage(target_ip, gpuIndex);
        metrics["GpuPowerUsage"] = mpiGetGpuPowerUsage(target_ip, gpuIndex);
        metrics["GpuFanSpeed"] = mpiGetGpuFanSpeed(target_ip, gpuIndex);
        metrics["GpuCoreClock"] = mpiGetGpuCoreClock(target_ip, gpuIndex);
        metrics["GpuMemoryClock"] = mpiGetGpuMemoryClock(target_ip, gpuIndex);
        return metrics;
    });
}

double MPIController::mpiGetAvailableMemory(const std::string& target_ip) {
    return mpiWrapperFunction(target_ip, SystemMetrics::getAvailableMemory);
}

// New MPI wrapper function for getCpuTemperature
double MPIController::getCpuTemperatureMPI(const std::string& ipAddress, const std::string& processName) {
    // Assuming the MPI wrapper function is implemented correctly to call the SystemMetrics function
    return mpiWrapperFunction(ipAddress, SystemMetrics::getCpuTemperature);
}

double MPIController::getSwapUsageMPI(const std::string& ipAddress, const std::string& processName) {
    return mpiWrapperFunction(ipAddress, SystemMetrics::getSwapUsage);
}

double MPIController::getMemoryPageFaultsMPI(const std::string& ipAddress, const std::string& processName) {
    return mpiWrapperFunction(ipAddress, SystemMetrics::getMemoryPageFaults);
}

double MPIController::getDiskLatencyMPI(const std::string& ipAddress, const std::string& processName, const std::string& disk) {
    return mpiWrapperFunction(ipAddress, SystemMetrics::getDiskLatency, disk);
}

double MPIController::getDiskSpaceUtilizationMPI(const std::string& ipAddress, const std::string& processName, const std::string& path) {
    return mpiWrapperFunction(ipAddress, SystemMetrics::getDiskSpaceUtilization, path);
}

double MPIController::getNetworkBandwidthUtilizationMPI(const std::string& ipAddress, const std::string& processName, const std::string& interface) {
    return mpiWrapperFunction(ipAddress, SystemMetrics::getNetworkBandwidthUtilization, interface);
}

double MPIController::getNetworkLatencyMPI(const std::string& ipAddress, const std::string& processName, const std::string& interface) {
    return mpiWrapperFunction(ipAddress, SystemMetrics::getNetworkLatency, interface);
}

double MPIController::getNetworkErrorsMPI(const std::string& ipAddress, const std::string& processName, const std::string& interface) {
    return mpiWrapperFunction(ipAddress, SystemMetrics::getNetworkErrors, interface);
}

double MPIController::getPacketLossMPI(const std::string& ipAddress, const std::string& processName, const std::string& interface) {
    return mpiWrapperFunction(ipAddress, SystemMetrics::getPacketLoss, interface);
}

int MPIController::getActiveConnectionsMPI(const std::string& ipAddress, const std::string& processName, const std::string& interface) {
    return mpiWrapperFunction(ipAddress, SystemMetrics::getActiveConnections, interface);
}

double MPIController::getApplicationResponseTimeMPI(const std::string& ipAddress, const std::string& processName, const std::string& app) {
    return mpiWrapperFunction(ipAddress, SystemMetrics::getApplicationResponseTime, app);
}

double MPIController::getApplicationErrorRateMPI(const std::string& ipAddress, const std::string& processName, const std::string& app) {
    return mpiWrapperFunction(ipAddress, SystemMetrics::getApplicationErrorRate, app);
}

double MPIController::getRequestRateMPI(const std::string& ipAddress, const std::string& processName, const std::string& app) {
    return mpiWrapperFunction(ipAddress, SystemMetrics::getRequestRate, app);
}

double MPIController::getThroughputMPI(const std::string& ipAddress, const std::string& processName, const std::string& app) {
    return mpiWrapperFunction(ipAddress, SystemMetrics::getThroughput, app);
}

double MPIController::getQueryPerformanceMPI(const std::string& ipAddress, const std::string& processName, const std::string& db) {
    return mpiWrapperFunction(ipAddress, SystemMetrics::getQueryPerformance, db);
}

double MPIController::getConnectionPoolUtilizationMPI(const std::string& ipAddress, const std::string& processName, const std::string& db) {
    return mpiWrapperFunction(ipAddress, SystemMetrics::getConnectionPoolUtilization, db);
}

double MPIController::getCacheHitMissRateMPI(const std::string& ipAddress, const std::string& processName, const std::string& db) {
    return mpiWrapperFunction(ipAddress, SystemMetrics::getCacheHitMissRate, db);
}

double MPIController::getTransactionRateMPI(const std::string& ipAddress, const std::string& processName, const std::string& db) {
    return mpiWrapperFunction(ipAddress, SystemMetrics::getTransactionRate, db);
}

int MPIController::getFailedLoginAttemptsMPI(const std::string& ipAddress, const std::string& processName) {
    return mpiWrapperFunction(ipAddress, SystemMetrics::getFailedLoginAttempts);
}

int MPIController::getIntrusionDetectionAlertsMPI(const std::string& ipAddress, const std::string& processName) {
    return mpiWrapperFunction(ipAddress, SystemMetrics::getIntrusionDetectionAlerts);
}

int MPIController::getFirewallLogEntriesMPI(const std::string& ipAddress, const std::string& processName) {
    return mpiWrapperFunction(ipAddress, SystemMetrics::getFirewallLogEntries);
}

int MPIController::getVulnerabilityScansMPI(const std::string& ipAddress, const std::string& processName) {
    return mpiWrapperFunction(ipAddress, SystemMetrics::getVulnerabilityScans);
}

double MPIController::getInstanceTypeUtilizationMPI(const std::string& ipAddress, const std::string& processName) {
    return mpiWrapperFunction(ipAddress, SystemMetrics::getInstanceTypeUtilization);
}

/// aasdf
double MPIController::getInstanceTypeUtilizationMPI(const std::string& ipAddress, const std::string& processName) {
    return mpiWrapperFunction(ipAddress, SystemMetrics::getInstanceTypeUtilization);
}

double MPIController::getAutoScalingMetricsMPI(const std::string& ipAddress, const std::string& processName) {
    return mpiWrapperFunction(ipAddress, SystemMetrics::getAutoScalingMetrics);
}

double MPIController::getResourceReservationsMPI(const std::string& ipAddress, const std::string& processName) {
    return mpiWrapperFunction(ipAddress, SystemMetrics::getResourceReservations);
}

double MPIController::getPowerUsageMPI(const std::string& ipAddress, const std::string& processName) {
    return mpiWrapperFunction(ipAddress, SystemMetrics::getPowerUsage);
}

double MPIController::getEnergyEfficiencyMPI(const std::string& ipAddress, const std::string& processName) {
    return mpiWrapperFunction(ipAddress, SystemMetrics::getEnergyEfficiency);
}

bool MPIController::shouldScaleMPI(const std::string& ipAddress, const std::string& instance, const std::string& app, const std::string& disk, const std::string& interface) {
    return mpiWrapperFunction(ipAddress, SystemMetrics::shouldScale, instance, app, disk, interface);
}