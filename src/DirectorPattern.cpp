#include "DirectorPattern.h"

class Director {	
	
Director::Director() {
    // Initialize instance variables
    ipAddress = SystemMetrics::getIpAddress();

    // Calls a C++ program in the directory that checks the status of intel tbb
    tbbAvailable = SystemMetrics::isTbbAvailable();

    // Calls a C++ program in the directory that checks the status of nvidia cuda
    cudaAvailable = SystemMetrics::hasNvidiaGpu();
}

	// If tbb and cuda are available return true
	int Director::stateMachine() {
		int state = 0;
		if (tbb == true) {
			state = state + 1;
		} else if (cuda == true) {
			state = state + 1;
		}
		return state;
	}

seastar::future<> Director::nodeController() {
    // Call NodeManager::monitorNodes periodically
    return seastar::repeat([this] {
        return nodeManager.monitorNodes().then([] {
            seastar::print("Monitoring nodes completed.\n");
            return seastar::sleep(std::chrono::seconds(10)); // Adjust the interval as needed
        }).handle_exception([] (std::exception_ptr ex) {
            seastar::print("Error during monitoring: %s\n", seastar::current_exception_as_string().c_str());
            return seastar::sleep(std::chrono::seconds(10)); // Adjust the interval as needed
        }).then([] {
            return seastar::stop_iteration::no;
        });
    });
}


	void Director::assembleGan() {
		/**
		 * Run all of the Machine Learning here, via seastar as if it were one program
		 * but call each of the processor-heavy functionalities such as Bayesian Regression, 
		 * PCA, Progressive Distillation, Temporal Coherence, LSTMs, Transformers, 
		 * Sigmoid functions, etc., from here
		 */ 
	}

	// The rest of the logic here for each functionality of the AI model

}