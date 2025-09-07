#ifndef GANCODIRECTOR_H
#define GANCODIRECTOR_H

#include <string>
#include <unordered_map>
#include <seastar/core/future.hh>

class GANCodirector {
public:
    GANCodirector();
    seastar::future<> train(const std::string& datasetPath);
    seastar::future<> generate(const std::string& outputPath);
    seastar::future<> loadModel(const std::string& modelPath);
    seastar::future<> saveModel(const std::string& modelPath);
    seastar::future<> monitorResources();

private:
    void initializeGAN();
    seastar::future<> performTrainingStep();
    seastar::future<> performInferenceStep();

    std::unordered_map<std::string, float> resourceMetrics;
    seastar::future<> checkAndScale();
};

#endif // GANCODIRECTOR_H
