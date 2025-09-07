#ifndef SYSTEM_METRICS_H
#define SYSTEM_METRICS_H

#include <string>
#include <vector>
#include <map>

class SystemMetrics {
public:
    double getCpuUtilization();
    std::vector<double> getCpuLoadAverage();
    double getCpuStealTime();
    double getCpuTemperature();

    double getMemoryUtilization();
    double getAvailableMemory();
    double getSwapUsage();
    double getMemoryPageFaults();

    double getDiskIoUtilization(const std::string& disk);
    double getDiskReadThroughput(const std::string& disk);
    double getDiskWriteThroughput(const std::string& disk);
    double getDiskLatency(const std::string& disk);
    double getDiskSpaceUtilization(const std::string& path);

    double getNetworkBandwidthUtilization(const std::string& interface);
    double getNetworkLatency(const std::string& interface);
    double getNetworkErrors(const std::string& interface);
    double getPacketLoss(const std::string& interface);
    int getActiveConnections(const std::string& interface);

    double getApplicationResponseTime(const std::string& app);
    double getApplicationErrorRate(const std::string& app);
    double getRequestRate(const std::string& app);
    double getThroughput(const std::string& app);

    double getQueryPerformance(const std::string& db);
    double getConnectionPoolUtilization(const std::string& db);
    double getCacheHitMissRate(const std::string& db);
    double getTransactionRate(const std::string& db);

    int getFailedLoginAttempts();
    int getIntrusionDetectionAlerts();
    int getFirewallLogEntries();
    int getVulnerabilityScans();

    double getInstanceTypeUtilization();
    double getAutoScalingMetrics();
    double getResourceReservations();

    double getPowerUsage();
    double getEnergyEfficiency();
    bool shouldScale(SystemMetrics& sysMetrics, const std::string& instance, const std::string& app, const std::string& disk, const std::string& interface);
    std::string getIpAddress();
    bool hasNvidiaGpu();
    bool isTbbAvailable();
    bool isMpiAvailable();

    struct ProcessMemoryInfo {
        std::string processName;
        double memoryUsageMB;
    };

    std::vector<ProcessMemoryInfo> getProcessMemoryUsage(const std::vector<pid_t>& pids);

    // GPU Methods
    float getGpuTemperature(unsigned int gpuIndex);
    float getGpuUsage(unsigned int gpuIndex);
    float getGpuMemoryUsage(unsigned int gpuIndex);

    float getGpuPowerUsage(unsigned int gpuIndex);
    float getGpuFanSpeed(unsigned int gpuIndex);
    float getGpuCoreClock(unsigned int gpuIndex);
    float getGpuMemoryClock(unsigned int gpuIndex);
    float getGpuMemoryBandwidthUsage(unsigned int gpuIndex);
    std::string getGpuComputeMode(unsigned int gpuIndex);
    float getGpuPCIeThroughput(unsigned int gpuIndex, nvmlPcieUtilCounter_t counter);    

private:
    double readStatFile();
    double readProcFile(const std::string& path);
    std::map<std::string, double> readNetDevFile();
    std::string detectDatabaseType(const std::string& db);
    void initializeNVML();
    void shutdownNVML();

};

#endif // SYSTEM_METRICS_H