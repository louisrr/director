#include "GpuMetrics.h"

GpuMetrics::GpuMetrics() {
    initNVML();
}

GpuMetrics::~GpuMetrics() {
    shutdownNVML();
}

void GpuMetrics::initNVML() {
    nvmlReturn_t result = nvmlInit();
    if (NVML_SUCCESS != result) {
        throw std::runtime_error("Failed to initialize NVML: " + std::string(nvmlErrorString(result)));
    }
}

void GpuMetrics::shutdownNVML() {
    nvmlReturn_t result = nvmlShutdown();
    if (NVML_SUCCESS != result) {
        std::cerr << "Failed to shutdown NVML: " + std::string(nvmlErrorString(result)) << std::endl;
    }
}

double GpuMetrics::getTemperature(int deviceIndex) {
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

double GpuMetrics::getGpuUtilization(int deviceIndex) {
    nvmlDevice_t device;
    nvmlReturn_t result = nvmlDeviceGetHandleByIndex(deviceIndex, &device);
    if (NVML_SUCCESS != result) {
        throw std::runtime_error("Failed to get handle for device " + std::to_string(deviceIndex) + ": " + std::string(nvmlErrorString(result)));
    }

    nvmlUtilization_t utilization;
    result = nvmlDeviceGetUtilizationRates(device, &utilization);
    if (NVML_SUCCESS != result) {
        throw std::runtime_error("Failed to get GPU utilization for device " + std::to_string(deviceIndex) + ": " + std::string(nvmlErrorString(result)));
    }

    return static_cast<double>(utilization.gpu);
}

double GpuMetrics::getMemoryUtilization(int deviceIndex) {
    nvmlDevice_t device;
    nvmlReturn_t result = nvmlDeviceGetHandleByIndex(deviceIndex, &device);
    if (NVML_SUCCESS != result) {
        throw std::runtime_error("Failed to get handle for device " + std::to_string(deviceIndex) + ": " + std::string(nvmlErrorString(result)));
    }

    nvmlMemory_t memory;
    result = nvmlDeviceGetMemoryInfo(device, &memory);
    if (NVML_SUCCESS != result) {
        throw std::runtime_error("Failed to get memory utilization for device " + std::to_string(deviceIndex) + ": " + std::string(nvmlErrorString(result)));
    }

    return static_cast<double>(memory.used) / static_cast<double>(memory.total) * 100.0;
}

double GpuMetrics::getPowerUsage(int deviceIndex) {
    nvmlDevice_t device;
    nvmlReturn_t result = nvmlDeviceGetHandleByIndex(deviceIndex, &device);
    if (NVML_SUCCESS != result) {
        throw std::runtime_error("Failed to get handle for device " + std::to_string(deviceIndex) + ": " + std::string(nvmlErrorString(result)));
    }

    unsigned int power;
    result = nvmlDeviceGetPowerUsage(device, &power);
    if (NVML_SUCCESS != result) {
        throw std::runtime_error("Failed to get power usage for device " + std::to_string(deviceIndex) + ": " + std::string(nvmlErrorString(result)));
    }

    return static_cast<double>(power) / 1000.0; // Convert from milliwatts to watts
}

double GpuMetrics::getFanSpeed(int deviceIndex) {
    nvmlDevice_t device;
    nvmlReturn_t result = nvmlDeviceGetHandleByIndex(deviceIndex, &device);
    if (NVML_SUCCESS != result) {
        throw std::runtime_error("Failed to get handle for device " + std::to_string(deviceIndex) + ": " + std::string(nvmlErrorString(result)));
    }

    unsigned int speed;
    result = nvmlDeviceGetFanSpeed(device, &speed);
    if (NVML_SUCCESS != result) {
        throw std::runtime_error("Failed to get fan speed for device " + std::to_string(deviceIndex) + ": " + std::string(nvmlErrorString(result)));
    }

    return static_cast<double>(speed);
}

double GpuMetrics::getClockSpeed(int deviceIndex, nvmlClockType_t clockType) {
    nvmlDevice_t device;
    nvmlReturn_t result = nvmlDeviceGetHandleByIndex(deviceIndex, &device);
    if (NVML_SUCCESS != result) {
        throw std::runtime_error("Failed to get handle for device " + std::to_string(deviceIndex) + ": " + std::string(nvmlErrorString(result)));
    }

    unsigned int clockSpeed;
    result = nvmlDeviceGetClock(device, clockType, &clockSpeed);
    if (NVML_SUCCESS != result) {
        throw std::runtime_error("Failed to get clock speed for device " + std::to_string(deviceIndex) + ": " + std::string(nvmlErrorString(result)));
    }

    return static_cast<double>(clockSpeed);
}
