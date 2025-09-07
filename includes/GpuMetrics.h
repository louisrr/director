#ifndef GPU_METRICS_H
#define GPU_METRICS_H

#include <nvml.h>
#include <iostream>
#include <stdexcept>

class GpuMetrics {
public:
    GpuMetrics();
    ~GpuMetrics();
    double getTemperature(int deviceIndex);
    double getGpuUtilization(int deviceIndex);
    double getMemoryUtilization(int deviceIndex);
    double getPowerUsage(int deviceIndex);
    double getFanSpeed(int deviceIndex);
    double getClockSpeed(int deviceIndex, nvmlClockType_t clockType);

private:
    void initNVML();
    void shutdownNVML();
};

#endif // GPU_METRICS_H
