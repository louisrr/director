#include "NodeManager.h"


#include <redis/redis.h> // Assuming you are using a Redis library







NodeManager::NodeManager() {
    // Constructor implementation (if needed)
}

seastar::future<> NodeManager::monitorNodes() {
    return seastar::async([this] {
        // Loop through all registered nodes to monitor and scale as needed
        return registeredNodes.maps.map_reduce0(
            [this](auto& map) {
                std::vector<seastar::future<>> futures;
                for (const auto& [ipAddress, processName] : map) {
                    futures.push_back(
                        mpiController.gpuMetrics(ipAddress, 0).then([this, ipAddress, processName](std::unordered_map<std::string, float> gpuMetrics) {
                            auto cpuUsage = seastar::engine().cpu_id();
                            auto memoryUsage = seastar::memory::stats();
                            auto hddUsage = 40.0; // Example value, replace with actual HDD usage query
                            auto trafficIn = 100.0; // Example value, replace with actual traffic input query
                            auto trafficOut = 150.0; // Example value, replace with actual traffic output query

                            seastar::vector<std::pair<std::string, float>> metrics;

                            metrics.emplace_back("CpuUsage", cpuUsage);
                            metrics.emplace_back("TotalMemory", memoryUsage.total_memory());
                            metrics.emplace_back("FreeMemory", memoryUsage.free_memory());
                            metrics.emplace_back("UsedMemory", memoryUsage.used_memory());
                            metrics.emplace_back("HddUsage", hddUsage);
                            metrics.emplace_back("TrafficIn", trafficIn);
                            metrics.emplace_back("TrafficOut", trafficOut);

                            for (const auto& metric : gpuMetrics) {
                                metrics.emplace_back(metric.first, metric.second);
                            }

                            for (const auto& metric : metrics) {
                                seastar::print("%s: %f\n", metric.first, metric.second);
                            }

                            // Append the current timestamp to the process name
                            auto timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
                            std::string newProcessName = processName + "_" + std::to_string(timestamp);

                            // Implement deterministic scaling logic based on collected metrics
                            return needsScaling(ipAddress, processName).then([this, ipAddress, newProcessName](bool scaleUpNeeded) {
                                if (scaleUpNeeded) {
                                    return queueToScaleUp.enqueue_back({ipAddress, newProcessName});
                                } else {
                                    return queueToScaleDown.enqueue_back({ipAddress, newProcessName});
                                }
                            });
                        })
                    );
                }
                return seastar::when_all(futures.begin(), futures.end()).discard_result();
            },
            std::vector<seastar::future<>>(),
            [](auto&& a, auto&& b) {
                a.insert(a.end(), std::make_move_iterator(b.begin()), std::make_move_iterator(b.end()));
                return std::move(a);
            }
        ).then([this] {
            // Process nodes that need to be scaled up
            return process_node(queueToScaleUp).then([this] {
                // Process nodes that need to be scaled down
                return process_node(queueToScaleDown);
            });
        });
    });
}

seastar::future<> NodeManager::process_node(NodeQueue& queue) {
    return seastar::repeat([&queue]() {
        return queue.dequeue_front().then([](Node node) {
            if (node.id == -1) {
                return seastar::stop_iteration::yes;  // Stop if the queue is empty
            }
            // Process the node (scale up, scale down, or delete)
            std::cout << "Processing Node " << node.id << "\n";
            // Implement the logic for scaling up or down the node
            return seastar::stop_iteration::no;
        });
    });
}

seastar::future<int> NodeManager::getNodeLoad(const std::string &ipAddress) {
    return seastar::async([this, ipAddress] {
        int totalLoad = 0;

        return mpiController.getCpuTemperatureMPI(ipAddress, "").then([&totalLoad](double cpuTemp) {
            totalLoad += static_cast<int>(cpuTemp);

            return mpiController.getMemoryPageFaultsMPI(ipAddress, "");
        }).then([&totalLoad](double memPageFaults) {
            totalLoad += static_cast<int>(memPageFaults);

            return mpiController.getNetworkBandwidthUtilizationMPI(ipAddress, "", "eth0");
        }).then([&totalLoad](double netBandwidth) {
            totalLoad += static_cast<int>(netBandwidth);

            return mpiController.getGpuUsage(ipAddress, 0); // Assuming GPU index 0
        }).then([&totalLoad](float gpuUsage) {
            totalLoad += static_cast<int>(gpuUsage);

            return seastar::make_ready_future<int>(totalLoad);
        });
    });
}

seastar::future<bool> NodeManager::needsScaling(const std::string &ipAddress, const std::string &processName) {
    const double CPU_TEMP_THRESHOLD = 80.0; // Example threshold in degrees Celsius
    const double MEM_PAGE_FAULTS_THRESHOLD = 1000.0; // Example threshold for page faults
    const double NET_BANDWIDTH_THRESHOLD = 1000.0; // Example threshold in MBps
    const double GPU_USAGE_THRESHOLD = 80.0; // Example threshold in percent
    const double MEM_AVAILABLE_THRESHOLD = 512.0; // Example threshold in MB
    const double DISK_LATENCY_THRESHOLD = 10.0; // Example threshold in ms

    return mpiController.getCpuTemperatureMPI(ipAddress, processName).then([this, ipAddress, processName](double cpuTemp) {
        if (cpuTemp > CPU_TEMP_THRESHOLD) {
            return seastar::make_ready_future<bool>(true);
        }

        return mpiController.getMemoryPageFaultsMPI(ipAddress, processName);
    }).then([this, ipAddress, processName](double memPageFaults) {
        if (memPageFaults > MEM_PAGE_FAULTS_THRESHOLD) {
            return seastar::make_ready_future<bool>(true);
        }

        return mpiController.getNetworkBandwidthUtilizationMPI(ipAddress, processName, "eth0");
    }).then([this, ipAddress, processName](double netBandwidth) {
        if (netBandwidth > NET_BANDWIDTH_THRESHOLD) {
            return seastar::make_ready_future<bool>(true);
        }

        return mpiController.gpuMetrics(ipAddress, 0); // Assuming GPU index 0
    }).then([this, ipAddress, processName](std::unordered_map<std::string, float> gpuMetrics) {
        if (gpuMetrics["GpuUsage"] > GPU_USAGE_THRESHOLD) {
            return seastar::make_ready_future<bool>(true);
        }

        return mpiController.mpiGetAvailableMemory(ipAddress);
    }).then([this, ipAddress, processName](double memAvailable) {
        if (memAvailable < MEM_AVAILABLE_THRESHOLD) {
            return seastar::make_ready_future<bool>(true);
        }

        return mpiController.getDiskLatencyMPI(ipAddress, processName, "/dev/sda"); // Example disk path
    }).then([this](double diskLatency) {
        if (diskLatency > DISK_LATENCY_THRESHOLD) {
            return seastar::make_ready_future<bool>(true);
        }

        return seastar::make_ready_future<bool>(false); // No scaling needed
    });
}

seastar::future<bool> NodeManager::needsTermination() {
    return seastar::async([this] {
        // Check if any nodes should be terminated
        return false; // Example logic
    });
}

seastar::future<> NodeManager::gracefulShutdown(const std::string &processName) {
    // Implement logic to gracefully shut down the process
    return seastar::async([processName] {
        // Example logic to signal the process to shut down
        // You might use inter-process communication (IPC) or other signaling methods
        seastar::print("Gracefully shutting down process: %s\n", processName);
    });
}

seastar::future<> NodeManager::scaleUp(const std::string &processName, const std::string &cloudProvider) {
    return needsScaling(processName).then([this, processName, cloudProvider](bool scale) {
        if (scale) {
            return gracefulShutdown(processName).then([this, processName, cloudProvider] {
                std::string newProcessName = processName + "_new";
                std::string newIpAddress; // Get the new IP address after spinning up the instance
                return seastar::async([this, processName, cloudProvider, newProcessName, &newIpAddress] {
                    if (cloudProvider == "AWS") {
                        return scaleUpAWS(newProcessName).then([&newIpAddress](std::string ip) {
                            newIpAddress = ip;
                        });
                    } else if (cloudProvider == "Paperspace") {
                        return scaleUpPaperspace(newProcessName).then([&newIpAddress](std::string ip) {
                            newIpAddress = ip;
                        });
                    } else if (cloudProvider == "Nebius") {
                        return scaleUpNebius(newProcessName).then([&newIpAddress](std::string ip) {
                            newIpAddress = ip;
                        });
                    } else if (cloudProvider == "Azure") {
                        return scaleUpAzure(newProcessName).then([&newIpAddress](std::string ip) {
                            newIpAddress = ip;
                        });
                    } else if (cloudProvider == "GCP") {
                        return scaleUpGCP(newProcessName).then([&newIpAddress](std::string ip) {
                            newIpAddress = ip;
                        });
                    } else {
                        seastar::print("Unsupported cloud provider: %s\n", cloudProvider);
                        return seastar::make_ready_future<>();
                    }
                }).then([this, processName, newProcessName, newIpAddress] {
                    return updateProcessInfo(processName, newProcessName, newIpAddress);
                });
            });
        }
        return seastar::make_ready_future<>();
    });
}


seastar::future<> NodeManager::scaleUpAWS(const std::string &processName) {
    return seastar::async([this, processName] {
        seastar::print("Scaling up on AWS for process: %s\n", processName);

        Aws::SDKOptions options;
        Aws::InitAPI(options);
        {
            Aws::EC2::EC2Client ec2;
            Aws::EC2::Model::RunInstancesRequest runInstancesRequest;
            runInstancesRequest.SetImageId("ami-0abcdef1234567890"); // Example AMI ID
            runInstancesRequest.SetInstanceType(Aws::EC2::Model::InstanceType::t2_micro);
            runInstancesRequest.SetMinCount(1);
            runInstancesRequest.SetMaxCount(1);

            auto runInstancesOutcome = ec2.RunInstances(runInstancesRequest);
            if (!runInstancesOutcome.IsSuccess()) {
                seastar::print("Failed to start instance: %s\n", runInstancesOutcome.GetError().GetMessage().c_str());
            } else {
                const auto &instances = runInstancesOutcome.GetResult().GetInstances();
                for (const auto &instance : instances) {
                    seastar::print("Successfully started instance with ID: %s\n", instance.GetInstanceId().c_str());
                }
            }
        }
        Aws::ShutdownAPI(options);
    });
}

seastar::future<> NodeManager::scaleUpPaperspace(const std::string &processName) {
    return seastar::async([this, processName] {
        seastar::print("Scaling up on Paperspace for process: %s\n", processName);

        CURL *curl;
        CURLcode res;
        std::string readBuffer;

        curl = curl_easy_init();
        if(curl) {
            struct curl_slist *headers = NULL;
            headers = curl_slist_append(headers, "Content-Type: application/json");
            headers = curl_slist_append(headers, "Authorization: Bearer YOUR_PAPERSPACE_API_KEY");

            Json::Value requestData;
            requestData["region"] = "East Coast (NY2)";
            requestData["machineType"] = "C1";
            requestData["size"] = "50"; // Size in GB
            requestData["billingType"] = "hourly";
            requestData["templateId"] = "your-template-id"; // Use a valid template ID

            Json::StreamWriterBuilder writer;
            std::string requestBody = Json::writeString(writer, requestData);

            curl_easy_setopt(curl, CURLOPT_URL, "https://api.paperspace.io/machines/createSingleMachinePublic");
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, requestBody.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

            res = curl_easy_perform(curl);
            if(res != CURLE_OK) {
                seastar::print("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            } else {
                seastar::print("Paperspace response: %s\n", readBuffer.c_str());
            }

            curl_easy_cleanup(curl);
        }
    });
}

seastar::future<> NodeManager::scaleUpNebius(const std::string &processName) {
    return seastar::async([this, processName] {
        seastar::print("Scaling up on Nebius for process: %s\n", processName);

        CURL *curl;
        CURLcode res;
        std::string readBuffer;

        curl = curl_easy_init();
        if (curl) {
            struct curl_slist *headers = NULL;
            headers = curl_slist_append(headers, "Content-Type: application/json");
            headers = curl_slist_append(headers, "Authorization: Bearer YOUR_NEBIUS_API_KEY");

            Json::Value requestData;
            requestData["region"] = "your-region"; // Replace with your region
            requestData["instanceType"] = "your-instance-type"; // Replace with your instance type
            requestData["imageId"] = "your-image-id"; // Replace with your image ID
            requestData["name"] = processName;

            Json::StreamWriterBuilder writer;
            std::string requestBody = Json::writeString(writer, requestData);

            curl_easy_setopt(curl, CURLOPT_URL, "https://api.nebius.ai/v1/instances");
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, requestBody.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

            res = curl_easy_perform(curl);
            if (res != CURLE_OK) {
                seastar::print("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            } else {
                seastar::print("Nebius response: %s\n", readBuffer.c_str());
            }

            curl_easy_cleanup(curl);
        }
    });
}

seastar::future<> NodeManager::scaleUpAzure(const std::string &processName) {
    return seastar::async([this, processName] {
        seastar::print("Scaling up on Azure for process: %s\n", processName);

        // Initialize the Azure SDK
        auto credential = std::make_shared<Azure::Identity::DefaultAzureCredential>();

        Azure::Core::Context context;
        Azure::Compute::ComputeManagementClient client(credential, "your-subscription-id");

        Azure::Compute::Models::VirtualMachine vm;
        vm.location = "your-region"; // Replace with your region
        vm.osProfile.adminUsername = "your-username"; // Replace with your admin username
        vm.osProfile.adminPassword = "your-password"; // Replace with your admin password
        vm.osProfile.computerName = processName;

        Azure::Compute::Models::HardwareProfile hwProfile;
        hwProfile.vmSize = "Standard_DS1_v2"; // Replace with your VM size
        vm.hardwareProfile = hwProfile;

        Azure::Compute::Models::StorageProfile storageProfile;
        Azure::Compute::Models::ImageReference imageRef;
        imageRef.publisher = "Canonical";
        imageRef.offer = "UbuntuServer";
        imageRef.sku = "18.04-LTS";
        imageRef.version = "latest";
        storageProfile.imageReference = imageRef;
        vm.storageProfile = storageProfile;

        auto createResult = client.VirtualMachines.StartCreateOrUpdate(
            context, "your-resource-group", processName, vm);

        if (!createResult.Value().IsSuccessStatusCode()) {
            seastar::print("Failed to start instance: %s\n", createResult.Value().ReasonPhrase.c_str());
        } else {
            seastar::print("Successfully started instance: %s\n", processName);
        }
    });
}


seastar::future<> NodeManager::scaleUpGCP(const std::string &processName) {
    return seastar::async([this, processName] {
        seastar::print("Scaling up on GCP for process: %s\n", processName);

        namespace gcp = google::cloud::compute::v1;
        auto client = gcp::InstancesClient::CreateDefaultClient().value();

        gcp::InsertInstanceRequest request;
        request.set_project("your-project-id"); // Replace with your project ID
        request.set_zone("us-central1-a"); // Replace with your zone
        request.mutable_instance()->set_name(processName);
        request.mutable_instance()->set_machine_type("zones/us-central1-a/machineTypes/n1-standard-1");

        auto& network_interface = *request.mutable_instance()->add_network_interfaces();
        network_interface.set_name("global/networks/default");

        auto& disk = *request.mutable_instance()->add_disks();
        disk.set_boot(true);
        disk.mutable_initialize_params()->set_source_image("projects/debian-cloud/global/images/family/debian-10"); // Replace with your image

        auto status = client.Insert(request);
        if (!status.ok()) {
            seastar::print("Failed to start instance: %s\n", status.message());
        } else {
            seastar::print("Successfully started instance: %s\n", processName);
        }
    });
}


seastar::future<> NodeManager::registerNode(const std::string &ipAddress, const std::string &nodeName) {
    return seastar::async([this, ipAddress, nodeName] {
        // Add ipAddress and nodeName to the registeredNodes DistributedLinkedHashMap
        registeredNodes.put(ipAddress, nodeName).get(); // Ensure to use .get() to wait for the future to complete
        seastar::print("Registering node: %s with IP: %s\n", nodeName, ipAddress);
    });
}

seastar::future<> NodeManager::unregisterNode(const std::string &ipAddress) {
    return seastar::async([this, ipAddress] {
        // Remove ipAddress from the registeredNodes DistributedLinkedHashMap
        registeredNodes.remove(ipAddress).get(); // Ensure to use .get() to wait for the future to complete
        seastar::print("Unregistering node with IP: %s\n", ipAddress);
    });
}

seastar::future<> NodeManager::loadBalancer() {
    return seastar::async([this] {
        seastar::print("Balancing load across nodes...\n");

        // Example load balancing logic using CHBL

        std::unordered_map<std::string, int> nodeLoads;
        std::vector<std::string> nodes;

        // Fetch loads for all nodes
        return registeredNodes.maps.map_reduce0(
            [this](auto& map) {
                std::vector<seastar::future<std::pair<std::string, int>>> loadFutures;
                for (const auto& [ip, name] : map) {
                    loadFutures.push_back(getNodeLoad(ip).then([ip](int load) {
                        return std::make_pair(ip, load);
                    }));
                }
                return seastar::when_all_succeed(loadFutures.begin(), loadFutures.end());
            },
            std::vector<std::pair<std::string, int>>(),
            [](auto&& a, auto&& b) {
                a.insert(a.end(), std::make_move_iterator(b.begin()), std::make_move_iterator(b.end()));
                return std::move(a);
            }
        ).then([this, &nodeLoads, &nodes](std::vector<std::pair<std::string, int>> loads) {
            for (const auto& [ip, load] : loads) {
                nodeLoads[ip] = load;
                nodes.push_back(ip);
            }

            // Sort nodes by load
            std::sort(nodes.begin(), nodes.end(), [&nodeLoads](const std::string& a, const std::string& b) {
                return nodeLoads[a] < nodeLoads[b];
            });

            // Implement CHBL logic here
            // Distribute processes to nodes with consideration to load bounds

            // For demonstration, let's assume we simply print the loads
            for (const auto& node : nodes) {
                seastar::print("Node %s has load %d\n", node, nodeLoads[node]);
            }
            return seastar::make_ready_future<>();
        });
    });
}

seastar::future<> NodeManager::deleteNodeAWS(const std::string &instanceId) {
    return seastar::async([this, instanceId] {
        seastar::print("Deleting AWS instance: %s\n", instanceId);

        Aws::SDKOptions options;
        Aws::InitAPI(options);
        {
            Aws::EC2::EC2Client ec2;
            Aws::EC2::Model::TerminateInstancesRequest terminateRequest;
            terminateRequest.AddInstanceIds(instanceId);

            auto terminateOutcome = ec2.TerminateInstances(terminateRequest);
            if (!terminateOutcome.IsSuccess()) {
                seastar::print("Failed to terminate instance: %s\n", terminateOutcome.GetError().GetMessage().c_str());
            } else {
                seastar::print("Successfully terminated instance: %s\n", instanceId);
            }
        }
        Aws::ShutdownAPI(options);
    });
}

seastar::future<> NodeManager::deleteNodePaperspace(const std::string &instanceId) {
    return seastar::async([this, instanceId] {
        seastar::print("Deleting Paperspace instance: %s\n", instanceId);

        CURL *curl;
        CURLcode res;
        std::string readBuffer;

        curl = curl_easy_init();
        if (curl) {
            struct curl_slist *headers = NULL;
            headers = curl_slist_append(headers, "Content-Type: application/json");
            headers = curl_slist_append(headers, "Authorization: Bearer YOUR_PAPERSPACE_API_KEY");

            std::string url = "https://api.paperspace.io/machines/" + instanceId + "/destroyMachine";

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

            res = curl_easy_perform(curl);
            if (res != CURLE_OK) {
                seastar::print("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            } else {
                seastar::print("Paperspace response: %s\n", readBuffer.c_str());
            }

            curl_easy_cleanup(curl);
        }
    });
}

seastar::future<> NodeManager::deleteNodeNebius(const std::string &instanceId) {
    return seastar::async([this, instanceId] {
        seastar::print("Deleting Nebius instance: %s\n", instanceId);

        CURL *curl;
        CURLcode res;
        std::string readBuffer;

        curl = curl_easy_init();
        if (curl) {
            struct curl_slist *headers = NULL;
            headers = curl_slist_append(headers, "Content-Type: application/json");
            headers = curl_slist_append(headers, "Authorization: Bearer YOUR_NEBIUS_API_KEY");

            std::string url = "https://api.nebius.ai/v1/instances/" + instanceId;

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

            res = curl_easy_perform(curl);
            if (res != CURLE_OK) {
                seastar::print("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            } else {
                seastar::print("Nebius response: %s\n", readBuffer.c_str());
            }

            curl_easy_cleanup(curl);
        }
    });
}

seastar::future<> NodeManager::deleteNodeAzure(const std::string &instanceId) {
    return seastar::async([this, instanceId] {
        seastar::print("Deleting Azure instance: %s\n", instanceId);

        auto credential = std::make_shared<Azure::Identity::DefaultAzureCredential>();

        Azure::Core::Context context;
        Azure::Compute::ComputeManagementClient client(credential, "your-subscription-id");

        auto deleteResult = client.VirtualMachines.StartDelete(
            context, "your-resource-group", instanceId);

        if (!deleteResult.Value().IsSuccessStatusCode()) {
            seastar::print("Failed to delete instance: %s\n", deleteResult.Value().ReasonPhrase.c_str());
        } else {
            seastar::print("Successfully deleted instance: %s\n", instanceId);
        }
    });
}

seastar::future<> NodeManager::deleteNodeGCP(const std::string &instanceId) {
    return seastar::async([this, instanceId] {
        seastar::print("Deleting GCP instance: %s\n", instanceId);

        namespace gcp = google::cloud::compute::v1;
        auto client = gcp::InstancesClient::CreateDefaultClient().value();

        gcp::DeleteInstanceRequest request;
        request.set_project("your-project-id"); // Replace with your project ID
        request.set_zone("us-central1-a"); // Replace with your zone
        request.set_instance(instanceId);

        auto status = client.Delete(request);
        if (!status.ok()) {
            seastar::print("Failed to delete instance: %s\n", status.message());
        } else {
            seastar::print("Successfully deleted instance: %s\n", instanceId);
        }
    });
}

seastar::future<> NodeManager::deleteNode(const std::string &instanceId, const std::string &cloudProvider) {
    return seastar::async([this, instanceId, cloudProvider] {
        if (cloudProvider == "AWS") {
            return deleteNodeAWS(instanceId);
        } else if (cloudProvider == "Paperspace") {
            return deleteNodePaperspace(instanceId);
        } else if (cloudProvider == "Nebius") {
            return deleteNodeNebius(instanceId);
        } else if (cloudProvider == "Azure") {
            return deleteNodeAzure(instanceId);
        } else if (cloudProvider == "GCP") {
            return deleteNodeGCP(instanceId);
        } else {
            seastar::print("Unsupported cloud provider: %s\n", cloudProvider);
            return seastar::make_ready_future<>();
        }
    });
}

seastar::future<int> NodeManager::getProcessLoad(const std::string &processName) {
    // Example implementation to get the load of a process
    return seastar::async([this, processName] {
        int totalLoad = 0;

        // Replace with actual logic to get the load of the process
        // This could include CPU, memory, and other resource usage metrics
        return mpiController.getCpuUsageMPI(processName).then([&totalLoad](double cpuUsage) {
            totalLoad += static_cast<int>(cpuUsage);

            return mpiController.getMemoryUsageMPI(processName);
        }).then([&totalLoad](double memUsage) {
            totalLoad += static_cast<int>(memUsage);

            return mpiController.getNetworkUsageMPI(processName);
        }).then([&totalLoad](double netUsage) {
            totalLoad += static_cast<int>(netUsage);

            return seastar::make_ready_future<int>(totalLoad);
        });
    });
}

seastar::future<bool> NodeManager::needsScaling(const std::string &processName) {
    const double LOAD_THRESHOLD = 80.0; // Example threshold

    return getProcessLoad(processName).then([LOAD_THRESHOLD](int load) {
        return seastar::make_ready_future<bool>(load > LOAD_THRESHOLD);
    });
}

seastar::future<> NodeManager::updateProcessInfo(const std::string &oldProcessName, const std::string &newProcessName, const std::string &newIpAddress) {
    return seastar::async([this, oldProcessName, newProcessName, newIpAddress] {
        // Implement logic to update the process information
        registeredNodes.remove(oldProcessName).get(); // Remove the old process
        registeredNodes.put(newIpAddress, newProcessName).get(); // Add the new process
        seastar::print("Updated process info: %s -> %s with IP: %s\n", oldProcessName, newProcessName, newIpAddress);
    });
}
