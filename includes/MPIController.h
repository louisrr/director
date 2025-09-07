#ifndef MPICONTROLLER_H
#define MPICONTROLLER_H

#include <mpi.h>
#include <vector>
#include <string>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include "SystemMetrics.h" // Ensure this is the correct path to your SystemMetrics header
#include <unordered_map>
#include <seastar/core/future.hh>

class SystemMetricsMPI {
public: 
	// Function declaration for distributing and gathering memory usage
	void distributeAndGatherMemoryUsage(const std::vector<pid_t>& pids, int target_rank);

	// MPI wrapper GPU function declarations 
	float mpiGetGpuTemperature(const std::string& target_ip, unsigned int gpuIndex);
	float mpiGetGpuUsage(const std::string& target_ip, unsigned int gpuIndex);
	float mpiGetGpuMemoryUsage(const std::string& target_ip, unsigned int gpuIndex);
	float mpiGetGpuPowerUsage(const std::string& target_ip, unsigned int gpuIndex);
	float mpiGetGpuFanSpeed(const std::string& target_ip, unsigned int gpuIndex);
	float mpiGetGpuCoreClock(const std::string& target_ip, unsigned int gpuIndex);
	float mpiGetGpuMemoryClock(const std::string& target_ip, unsigned int gpuIndex);
	static seastar::future<std::unordered_map<std::string, float>> gpuMetrics(const std::string& target_ip, unsigned int gpuIndex);

	static double getAvailableMemoryMPI(const std::string& ipAddress, const std::string& processName);
	static double getCpuTemperatureMPI(const std::string& ipAddress, const std::string& processName);
    static double getSwapUsageMPI(const std::string& ipAddress, const std::string& processName);
    static double getMemoryPageFaultsMPI(const std::string& ipAddress, const std::string& processName);
    static double getDiskLatencyMPI(const std::string& ipAddress, const std::string& processName, const std::string& disk);
    static double getDiskSpaceUtilizationMPI(const std::string& ipAddress, const std::string& processName, const std::string& path);
    static double getNetworkBandwidthUtilizationMPI(const std::string& ipAddress, const std::string& processName, const std::string& interface);

    // New MPI wrapper functions
    static double getNetworkLatencyMPI(const std::string& ipAddress, const std::string& processName, const std::string& interface);
    static double getNetworkErrorsMPI(const std::string& ipAddress, const std::string& processName, const std::string& interface);
    static double getPacketLossMPI(const std::string& ipAddress, const std::string& processName, const std::string& interface);
    static int getActiveConnectionsMPI(const std::string& ipAddress, const std::string& processName, const std::string& interface);
    static double getApplicationResponseTimeMPI(const std::string& ipAddress, const std::string& processName, const std::string& app);
    static double getApplicationErrorRateMPI(const std::string& ipAddress, const std::string& processName, const std::string& app);
    static double getRequestRateMPI(const std::string& ipAddress, const std::string& processName, const std::string& app);
    static double getThroughputMPI(const std::string& ipAddress, const std::string& processName, const std::string& app);
    static double getQueryPerformanceMPI(const std::string& ipAddress, const std::string& processName, const std::string& db);
    static double getConnectionPoolUtilizationMPI(const std::string& ipAddress, const std::string& processName, const std::string& db);
    static double getCacheHitMissRateMPI(const std::string& ipAddress, const std::string& processName, const std::string& db);
    static double getTransactionRateMPI(const std::string& ipAddress, const std::string& processName, const std::string& db);
    static int getFailedLoginAttemptsMPI(const std::string& ipAddress, const std::string& processName);
    static int getIntrusionDetectionAlertsMPI(const std::string& ipAddress, const std::string& processName);
    static int getFirewallLogEntriesMPI(const std::string& ipAddress, const std::string& processName);
    static int getVulnerabilityScansMPI(const std::string& ipAddress, const std::string& processName);
    static double getInstanceTypeUtilizationMPI(const std::string& ipAddress, const std::string& processName);
    static double getInstanceTypeUtilizationMPI(const std::string& ipAddress, const std::string& processName);
    static double getAutoScalingMetricsMPI(const std::string& ipAddress, const std::string& processName);
    static double getResourceReservationsMPI(const std::string& ipAddress, const std::string& processName);
    static double getPowerUsageMPI(const std::string& ipAddress, const std::string& processName);
    static double getEnergyEfficiencyMPI(const std::string& ipAddress, const std::string& processName);

    static bool shouldScaleMPI(const std::string& ipAddress, const std::string& instance, const std::string& app, const std::string& disk, const std::string& interface);

    static double readStatFile();
    static double readProcFile(const std::string& path);
    static std::map<std::string, double> readNetDevFile();

	//MPI wrapper to get ram
	double mpiGetAvailableMemory();
private:
    static float mpiWrapperFunction(const std::string& target_ip, std::function<float(unsigned int)> func, unsigned int gpuIndex);
    static double mpiWrapperFunction(const std::string& target_ip, std::function<double()> func); // New wrapper for double functions

};

#endif // SYSTEM_METRICS_MPI_H