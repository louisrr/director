#include "SystemMetrics.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <iostream>
#include <tuple>
#include <array>
#include <memory>
#include <cstdio>
#include <string>
#include <vector>
#include <regex>
#include <libpq-fe.h>  // PostgreSQL
#include <mongoc/mongoc.h>  // MongoDB
#include <aerospike/aerospike.h>  // AerospikeDB
#include <scylladb-client/ScyllaDBClient.h>  // ScyllaDB

namespace helpers {
    std::tuple<long, long, long, long, long, long> readNetworkStats(const std::string& interface) {
        std::ifstream netDevFile("/proc/net/dev");
        if (!netDevFile.is_open()) {
            throw std::runtime_error("Failed to open /proc/net/dev");
        }

        std::string line;
        while (std::getline(netDevFile, line)) {
            if (line.find(interface) != std::string::npos) {
                std::istringstream iss(line);
                std::string iface;
                long rx_bytes, rx_packets, rx_errs, rx_drop, rx_fifo, rx_frame, rx_compressed, rx_multicast;
                long tx_bytes, tx_packets, tx_errs, tx_drop, tx_fifo, tx_colls, tx_carrier, tx_compressed;
                if (iss >> iface >> rx_bytes >> rx_packets >> rx_errs >> rx_drop >> rx_fifo >> rx_frame >> rx_compressed >> rx_multicast
                         >> tx_bytes >> tx_packets >> tx_errs >> tx_drop >> tx_fifo >> tx_colls >> tx_carrier >> tx_compressed) {
                    return std::make_tuple(rx_bytes, rx_packets, rx_errs, rx_drop, tx_errs, tx_drop);
                }
            }
        }

        throw std::runtime_error("Interface " + interface + " not found in /proc/net/dev");
    }

    double ping(const std::string& address) {
        std::array<char, 128> buffer;
        std::string result;
        std::shared_ptr<FILE> pipe(popen(("ping -c 1 " + address + " | grep 'time='").c_str(), "r"), pclose);
        if (!pipe) {
            throw std::runtime_error("Failed to run ping command");
        }
        while (fgets(buffer.data(), 128, pipe.get()) != nullptr) {
            result += buffer.data();
        }

        auto pos = result.find("time=");
        if (pos != std::string::npos) {
            auto start = pos + 5;
            auto end = result.find(" ms", start);
            return std::stod(result.substr(start, end - start));
        }

        throw std::runtime_error("Failed to parse ping result");
    }

    std::tuple<long, long> readCpuStats() {
        std::ifstream statFile("/proc/stat");
        if (!statFile.is_open()) {
            throw std::runtime_error("Failed to open /proc/stat");
        }

        std::string line;
        while (std::getline(statFile, line)) {
            std::istringstream iss(line);
            std::string cpu;
            long user, nice, system, idle, iowait, irq, softirq, steal;
            if (iss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal) {
                if (cpu == "cpu") {
                    long idleTime = idle + iowait;
                    long totalTime = user + nice + system + idle + iowait + irq + softirq + steal;
                    return std::make_tuple(idleTime, totalTime);
                }
            }
        }

        throw std::runtime_error("Failed to read CPU stats from /proc/stat");
    }

    long readCpuStealTime() {
        std::ifstream statFile("/proc/stat");
        if (!statFile.is_open()) {
            throw std::runtime_error("Failed to open /proc/stat");
        }

        std::string line;
        while (std::getline(statFile, line)) {
            std::istringstream iss(line);
            std::string cpu;
            long user, nice, system, idle, iowait, irq, softirq, steal;
            if (iss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal) {
                if (cpu == "cpu") {
                    return steal;
                }
            }
        }

        throw std::runtime_error("Failed to read CPU steal time from /proc/stat");
    }

    std::tuple<long, long> readDiskStats(const std::string& disk) {
        std::ifstream diskstatsFile("/proc/diskstats");
        if (!diskstatsFile.is_open()) {
            throw std::runtime_error("Failed to open /proc/diskstats");
        }

        std::string line;
        while (std::getline(diskstatsFile, line)) {
            std::istringstream iss(line);
            std::string device;
            long read_io, read_merges, read_sectors, read_time, write_io, write_merges, write_sectors, write_time, io_in_progress, io_time, weighted_io_time;
            if (iss >> device >> device >> device >> read_io >> read_merges >> read_sectors >> read_time >> write_io >> write_merges >> write_sectors >> write_time >> io_in_progress >> io_time >> weighted_io_time) {
                if (device == disk) {
                    return std::make_tuple(io_time, read_io + write_io);
                }
            }
        }

        throw std::runtime_error("Disk " + disk + " not found in /proc/diskstats");
    }

    long getSectorSize() {
        std::ifstream file("/sys/block/sda/queue/hw_sector_size");
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open /sys/block/sda/queue/hw_sector_size");
        }
        long sector_size;
        file >> sector_size;
        return sector_size;
    }

    double readCpuTemperature() {
        std::vector<std::string> temp_paths = {
            "/sys/class/thermal/thermal_zone0/temp", // Common path for CPU temperature
            "/sys/class/hwmon/hwmon0/temp1_input"   // Another common path
        };

        for (const auto& path : temp_paths) {
            std::ifstream tempFile(path);
            if (tempFile.is_open()) {
                long temp;
                tempFile >> temp;
                return static_cast<double>(temp) / 1000.0; // Convert from millidegrees to degrees
            }
        }

        throw std::runtime_error("Failed to read CPU temperature from known paths");
    }

    size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
        ((std::string*)userp)->append((char*)contents, size * nmemb);
        return size * nmemb;
    }

    std::string detectDatabaseType(const std::string& db) {
        // Implement logic to detect database type based on connection string or configuration
        // Placeholder example logic
        if (db.find("postgresql://") == 0) {
            return "PostgreSQL";
        } else if (db.find("mongodb://") == 0) {
            return "MongoDB";
        } else if (db.find("aerospike://") == 0) {
            return "AerospikeDB";
        } else if (db.find("scylla://") == 0) {
            return "ScyllaDB";
        }
        return "Unknown";
    }

    double getPostgresConnectionPoolUtilization(const std::string& db) {
        PGconn* conn = PQconnectdb(db.c_str());
        if (PQstatus(conn) != CONNECTION_OK) {
            PQfinish(conn);
            throw std::runtime_error("Failed to connect to PostgreSQL");
        }

        PGresult* res = PQexec(conn, "SELECT sum(numbackends)/count(*)::float AS pool_utilization FROM pg_stat_database");
        if (PQresultStatus(res) != PGRES_TUPLES_OK) {
            PQclear(res);
            PQfinish(conn);
            throw std::runtime_error("Failed to execute query on PostgreSQL");
        }

        double poolUtilization = std::stod(PQgetvalue(res, 0, 0));
        PQclear(res);
        PQfinish(conn);

        return poolUtilization;
    }

    double getMongoDBConnectionPoolUtilization(const std::string& db) {
        mongoc_client_t* client;
        bson_t* command;
        bson_t reply;
        bson_error_t error;
        bool ret;
        double poolUtilization = 0.0;

        mongoc_init();
        client = mongoc_client_new(db.c_str());
        if (!client) {
            throw std::runtime_error("Failed to connect to MongoDB");
        }

        command = BCON_NEW("serverStatus", BCON_INT32(1));

        ret = mongoc_client_command_simple(client, "admin", command, NULL, &reply, &error);
        if (ret) {
            bson_iter_t iter;
            if (bson_iter_init_find(&iter, &reply, "connections")) {
                bson_iter_t sub_iter;
                if (bson_iter_recurse(&iter, &sub_iter)) {
                    int current, available;
                    while (bson_iter_next(&sub_iter)) {
                        if (BSON_ITER_HOLDS_INT32(&sub_iter)) {
                            if (strcmp(bson_iter_key(&sub_iter), "current") == 0) {
                                current = bson_iter_int32(&sub_iter);
                            } else if (strcmp(bson_iter_key(&sub_iter), "available") == 0) {
                                available = bson_iter_int32(&sub_iter);
                            }
                        }
                    }
                    poolUtilization = (double)current / (current + available);
                }
            }
        } else {
            mongoc_client_destroy(client);
            mongoc_cleanup();
            bson_destroy(&reply);
            throw std::runtime_error("Failed to execute command on MongoDB");
        }

        mongoc_client_destroy(client);
        mongoc_cleanup();
        bson_destroy(&reply);
        bson_destroy(command);

        return poolUtilization;
    }

    double getAerospikeDBConnectionPoolUtilization(const std::string& db) {
        aerospike as;
        as_config config;
        as_error err;

        as_config_init(&config);
        if (!as_config_add_hosts(&config, db.c_str(), 3000)) {
            throw std::runtime_error("Failed to add AerospikeDB host");
        }

        aerospike_init(&as, &config);
        if (aerospike_connect(&as, &err) != AEROSPIKE_OK) {
            aerospike_destroy(&as);
            throw std::runtime_error("Failed to connect to AerospikeDB");
        }

        // Aerospike does not provide direct connection pool stats, assuming a fixed size pool
        double poolUtilization = 0.75;  // Placeholder value for demonstration

        aerospike_destroy(&as);
        return poolUtilization;
    }

    double getScyllaDBConnectionPoolUtilization(const std::string& db) {
        ScyllaDBClient client;
        if (!client.connect(db)) {
            throw std::runtime_error("Failed to connect to ScyllaDB");
        }

        // ScyllaDB does not provide direct connection pool stats, assuming a fixed size pool
        double poolUtilization = 0.65;  // Placeholder value for demonstration

        return poolUtilization;
    }
    double getPostgresCacheHitMissRate(const std::string& db) {
        PGconn* conn = PQconnectdb(db.c_str());
        if (PQstatus(conn) != CONNECTION_OK) {
            PQfinish(conn);
            throw std::runtime_error("Failed to connect to PostgreSQL");
        }

        PGresult* res = PQexec(conn, "SELECT sum(blks_hit) / (sum(blks_hit) + sum(blks_read))::float AS hit_miss_rate FROM pg_stat_database");
        if (PQresultStatus(res) != PGRES_TUPLES_OK) {
            PQclear(res);
            PQfinish(conn);
            throw std::runtime_error("Failed to execute query on PostgreSQL");
        }

        double hitMissRate = std::stod(PQgetvalue(res, 0, 0));
        PQclear(res);
        PQfinish(conn);

        return hitMissRate;
    }

    double getMongoDBCacheHitMissRate(const std::string& db) {
        mongoc_client_t* client;
        bson_t* command;
        bson_t reply;
        bson_error_t error;
        bool ret;
        double hitMissRate = 0.0;

        mongoc_init();
        client = mongoc_client_new(db.c_str());
        if (!client) {
            throw std::runtime_error("Failed to connect to MongoDB");
        }

        command = BCON_NEW("serverStatus", BCON_INT32(1));

        ret = mongoc_client_command_simple(client, "admin", command, NULL, &reply, &error);
        if (ret) {
            bson_iter_t iter;
            if (bson_iter_init_find(&iter, &reply, "wiredTiger")) {
                bson_iter_t sub_iter;
                if (bson_iter_recurse(&iter, &sub_iter)) {
                    while (bson_iter_next(&sub_iter)) {
                        if (BSON_ITER_HOLDS_DOCUMENT(&sub_iter) && strcmp(bson_iter_key(&sub_iter), "cache") == 0) {
                            bson_iter_t cache_iter;
                            if (bson_iter_recurse(&sub_iter, &cache_iter)) {
                                double cacheHits = 0, cacheMisses = 0;
                                while (bson_iter_next(&cache_iter)) {
                                    if (BSON_ITER_HOLDS_INT32(&cache_iter)) {
                                        if (strcmp(bson_iter_key(&cache_iter), "bytes read into cache") == 0) {
                                            cacheMisses = bson_iter_int32(&cache_iter);
                                        } else if (strcmp(bson_iter_key(&cache_iter), "bytes read from cache") == 0) {
                                            cacheHits = bson_iter_int32(&cache_iter);
                                        }
                                    }
                                }
                                hitMissRate = cacheHits / (cacheHits + cacheMisses);
                            }
                        }
                    }
                }
            }
        } else {
            mongoc_client_destroy(client);
            mongoc_cleanup();
            bson_destroy(&reply);
            throw std::runtime_error("Failed to execute command on MongoDB");
        }

        mongoc_client_destroy(client);
        mongoc_cleanup();
        bson_destroy(&reply);
        bson_destroy(command);

        return hitMissRate;
    }

    double getAerospikeDBCacheHitMissRate(const std::string& db) {
        aerospike as;
        as_config config;
        as_error err;
        as_record rec;

        as_config_init(&config);
        if (!as_config_add_hosts(&config, db.c_str(), 3000)) {
            throw std::runtime_error("Failed to add AerospikeDB host");
        }

        aerospike_init(&as, &config);
        if (aerospike_connect(&as, &err) != AEROSPIKE_OK) {
            aerospike_destroy(&as);
            throw std::runtime_error("Failed to connect to AerospikeDB");
        }

        // Aerospike does not provide direct cache stats, assuming a fixed cache hit/miss rate
        double hitMissRate = 0.80;  // Placeholder value for demonstration

        aerospike_destroy(&as);
        return hitMissRate;
    }

    double getScyllaDBCacheHitMissRate(const std::string& db) {
        ScyllaDBClient client;
        if (!client.connect(db)) {
            throw std::runtime_error("Failed to connect to ScyllaDB");
        }

        // ScyllaDB does not provide direct cache stats, assuming a fixed cache hit/miss rate
        double hitMissRate = 0.75;  // Placeholder value for demonstration

        return hitMissRate;
    }
    double getPostgresTransactionRate(const std::string& db) {
        PGconn* conn = PQconnectdb(db.c_str());
        if (PQstatus(conn) != CONNECTION_OK) {
            PQfinish(conn);
            throw std::runtime_error("Failed to connect to PostgreSQL");
        }

        PGresult* res = PQexec(conn, "SELECT xact_commit + xact_rollback AS transactions FROM pg_stat_database WHERE datname = current_database()");
        if (PQresultStatus(res) != PGRES_TUPLES_OK) {
            PQclear(res);
            PQfinish(conn);
            throw std::runtime_error("Failed to execute query on PostgreSQL");
        }

        long initialTransactions = std::stol(PQgetvalue(res, 0, 0));
        PQclear(res);

        std::this_thread::sleep_for(std::chrono::seconds(1));

        res = PQexec(conn, "SELECT xact_commit + xact_rollback AS transactions FROM pg_stat_database WHERE datname = current_database()");
        if (PQresultStatus(res) != PGRES_TUPLES_OK) {
            PQclear(res);
            PQfinish(conn);
            throw std::runtime_error("Failed to execute query on PostgreSQL");
        }

        long finalTransactions = std::stol(PQgetvalue(res, 0, 0));
        PQclear(res);
        PQfinish(conn);

        return static_cast<double>(finalTransactions - initialTransactions);
    }

    double getMongoDBTransactionRate(const std::string& db) {
        mongoc_client_t* client;
        bson_t* command;
        bson_t reply;
        bson_error_t error;
        bool ret;
        double transactionRate = 0.0;

        mongoc_init();
        client = mongoc_client_new(db.c_str());
        if (!client) {
            throw std::runtime_error("Failed to connect to MongoDB");
        }

        command = BCON_NEW("serverStatus", BCON_INT32(1));

        ret = mongoc_client_command_simple(client, "admin", command, NULL, &reply, &error);
        if (ret) {
            bson_iter_t iter;
            if (bson_iter_init_find(&iter, &reply, "opcounters")) {
                bson_iter_t sub_iter;
                if (bson_iter_recurse(&iter, &sub_iter)) {
                    int initialTransactions = 0;
                    while (bson_iter_next(&sub_iter)) {
                        if (BSON_ITER_HOLDS_INT32(&sub_iter)) {
                            if (strcmp(bson_iter_key(&sub_iter), "command") == 0) {
                                initialTransactions = bson_iter_int32(&sub_iter);
                            }
                        }
                    }

                    std::this_thread::sleep_for(std::chrono::seconds(1));

                    ret = mongoc_client_command_simple(client, "admin", command, NULL, &reply, &error);
                    if (ret && bson_iter_init_find(&iter, &reply, "opcounters")) {
                        if (bson_iter_recurse(&iter, &sub_iter)) {
                            int finalTransactions = 0;
                            while (bson_iter_next(&sub_iter)) {
                                if (BSON_ITER_HOLDS_INT32(&sub_iter)) {
                                    if (strcmp(bson_iter_key(&sub_iter), "command") == 0) {
                                        finalTransactions = bson_iter_int32(&sub_iter);
                                    }
                                }
                            }
                            transactionRate = static_cast<double>(finalTransactions - initialTransactions);
                        }
                    }
                }
            }
        } else {
            mongoc_client_destroy(client);
            mongoc_cleanup();
            bson_destroy(&reply);
            throw std::runtime_error("Failed to execute command on MongoDB");
        }

        mongoc_client_destroy(client);
        mongoc_cleanup();
        bson_destroy(&reply);
        bson_destroy(command);

        return transactionRate;
    }

    double getAerospikeDBTransactionRate(const std::string& db) {
        aerospike as;
        as_config config;
        as_error err;
        as_status status;
        as_record rec;

        as_config_init(&config);
        if (!as_config_add_hosts(&config, db.c_str(), 3000)) {
            throw std::runtime_error("Failed to add AerospikeDB host");
        }

        aerospike_init(&as, &config);
        if (aerospike_connect(&as, &err) != AEROSPIKE_OK) {
            aerospike_destroy(&as);
            throw std::runtime_error("Failed to connect to AerospikeDB");
        }

        // Aerospike does not provide direct transaction stats, using a placeholder value
        double transactionRate = 100.0;  // Placeholder value for demonstration

        aerospike_destroy(&as);
        return transactionRate;
    }

    double getScyllaDBTransactionRate(const std::string& db) {
        ScyllaDBClient client;
        if (!client.connect(db)) {
            throw std::runtime_error("Failed to connect to ScyllaDB");
        }

        // ScyllaDB does not provide direct transaction stats, using a placeholder value
        double transactionRate = 200.0;  // Placeholder value for demonstration

        return transactionRate;
    }

    int getFailedLoginAttempts() {
        std::ifstream logFile("/var/log/auth.log"); // For Ubuntu/Debian
        if (!logFile.is_open()) {
            logFile.open("/var/log/secure"); // For CentOS/Fedora/RHEL
            if (!logFile.is_open()) {
                throw std::runtime_error("Failed to open log file");
            }
        }

        std::string line;
        int failedAttempts = 0;
        std::regex regex("Failed password for");

        while (std::getline(logFile, line)) {
            if (std::regex_search(line, regex)) {
                ++failedAttempts;
            }
        }

        return failedAttempts;
    }

    int getIntrusionDetectionAlerts(const std::string& filePath) {
        std::ifstream logFile(filePath);
        if (!logFile.is_open()) {
            throw std::runtime_error("Failed to open intrusion detection log file");
        }

        std::string line;
        int alertCount = 0;

        while (std::getline(logFile, line)) {
            if (!line.empty()) { // Assuming each line represents an alert
                ++alertCount;
            }
        }

        return alertCount;
    } 

    int getFirewallLogEntries(const std::string& filePath) {
        std::ifstream logFile(filePath);
        if (!logFile.is_open()) {
            throw std::runtime_error("Failed to open firewall log file");
        }

        std::string line;
        int logCount = 0;

        while (std::getline(logFile, line)) {
            if (!line.empty()) { // Assuming each line represents a log entry
                ++logCount;
            }
        }

        return logCount;
    }    

    int getVulnerabilityScans(const std::string& filePath) {
        std::ifstream logFile(filePath);
        if (!logFile.is_open()) {
            throw std::runtime_error("Failed to open vulnerability scan log file");
        }

        std::string line;
        int scanCount = 0;

        while (std::getline(logFile, line)) {
            if (!line.empty()) { // Assuming each line represents a scan entry
                ++scanCount;
            }
        }

        return scanCount;
    }
    double getInstanceTypeUtilization(const std::string& filePath) {
        std::ifstream logFile(filePath);
        if (!logFile.is_open()) {
            throw std::runtime_error("Failed to open instance type utilization file");
        }

        std::string line;
        double totalUtilization = 0.0;
        int instanceCount = 0;

        while (std::getline(logFile, line)) {
            if (!line.empty()) {
                std::istringstream iss(line);
                double utilization;
                if (iss >> utilization) {
                    totalUtilization += utilization;
                    ++instanceCount;
                }
            }
        }

        if (instanceCount == 0) {
            throw std::runtime_error("No instance utilization data found");
        }

        return totalUtilization / instanceCount; // Average utilization
    }

    double getAutoScalingMetrics(const std::string& filePath) {
        std::ifstream logFile(filePath);
        if (!logFile.is_open()) {
            throw std::runtime_error("Failed to open auto-scaling metrics file");
        }

        std::string line;
        double totalScaleUpEvents = 0.0;
        double totalScaleDownEvents = 0.0;
        int scaleUpCount = 0;
        int scaleDownCount = 0;

        while (std::getline(logFile, line)) {
            if (!line.empty()) {
                std::istringstream iss(line);
                std::string eventType;
                double eventValue;
                if (iss >> eventType >> eventValue) {
                    if (eventType == "ScaleUp") {
                        totalScaleUpEvents += eventValue;
                        ++scaleUpCount;
                    } else if (eventType == "ScaleDown") {
                        totalScaleDownEvents += eventValue;
                        ++scaleDownCount;
                    }
                }
            }
        }

        if (scaleUpCount == 0 && scaleDownCount == 0) {
            throw std::runtime_error("No auto-scaling data found");
        }

        // Calculate a metric based on scale-up and scale-down events
        double scaleUpAverage = (scaleUpCount > 0) ? (totalScaleUpEvents / scaleUpCount) : 0.0;
        double scaleDownAverage = (scaleDownCount > 0) ? (totalScaleDownEvents / scaleDownCount) : 0.0;

        return (scaleUpAverage + scaleDownAverage) / 2.0; // Example: average of both events
    }

    double getResourceReservations(const std::string& filePath) {
        std::ifstream logFile(filePath);
        if (!logFile.is_open()) {
            throw std::runtime_error("Failed to open resource reservations file");
        }

        std::string line;
        double totalReservations = 0.0;
        int reservationCount = 0;

        while (std::getline(logFile, line)) {
            if (!line.empty()) {
                std::istringstream iss(line);
                double reservation;
                if (iss >> reservation) {
                    totalReservations += reservation;
                    ++reservationCount;
                }
            }
        }

        if (reservationCount == 0) {
            throw std::runtime_error("No resource reservation data found");
        }

        return totalReservations / reservationCount; // Average reservation
    }    

    double getPowerUsage(const std::string& filePath) {
        std::ifstream logFile(filePath);
        if (!logFile.is_open()) {
            throw std::runtime_error("Failed to open power usage file");
        }

        std::string line;
        double totalPowerUsage = 0.0;
        int usageCount = 0;

        while (std::getline(logFile, line)) {
            if (!line.empty()) {
                std::istringstream iss(line);
                double powerUsage;
                if (iss >> powerUsage) {
                    totalPowerUsage += powerUsage;
                    ++usageCount;
                }
            }
        }

        if (usageCount == 0) {
            throw std::runtime_error("No power usage data found");
        }

        return totalPowerUsage / usageCount; // Average power usage
    }

    double getEnergyEfficiency(const std::string& filePath) {
        std::ifstream logFile(filePath);
        if (!logFile.is_open()) {
            throw std::runtime_error("Failed to open energy efficiency file");
        }

        std::string line;
        double totalEfficiency = 0.0;
        int efficiencyCount = 0;

        while (std::getline(logFile, line)) {
            if (!line.empty()) {
                std::istringstream iss(line);
                double efficiency;
                if (iss >> efficiency) {
                    totalEfficiency += efficiency;
                    ++efficiencyCount;
                }
            }
        }

        if (efficiencyCount == 0) {
            throw std::runtime_error("No energy efficiency data found");
        }

        return totalEfficiency / efficiencyCount; // Average energy efficiency
    }        
}    


double SystemMetrics::getCpuUtilization() {
    auto [initialIdleTime, initialTotalTime] = helpers::readCpuStats();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    auto [finalIdleTime, finalTotalTime] = helpers::readCpuStats();

    long deltaIdle = finalIdleTime - initialIdleTime;
    long deltaTotal = finalTotalTime - initialTotalTime;

    return (1.0 - static_cast<double>(deltaIdle) / deltaTotal) * 100.0;
}

std::vector<double> SystemMetrics::getCpuLoadAverage() {
    std::vector<double> loadAverages(3, 0.0);
    if (getloadavg(loadAverages.data(), loadAverages.size()) == -1) {
        throw std::runtime_error("Failed to get load average");
    }
    return loadAverages;
}

double SystemMetrics::getDiskIoUtilization(const std::string& disk) {
    auto readDiskStats = [&disk]() -> long {
        std::ifstream diskstatsFile("/proc/diskstats");
        if (!diskstatsFile.is_open()) {
            throw std::runtime_error("Failed to open /proc/diskstats");
        }

        std::string line;
        while (std::getline(diskstatsFile, line)) {
            std::istringstream iss(line);
            std::string device;
            long read_io, read_merges, read_sectors, read_time, write_io, write_merges, write_sectors, write_time, io_in_progress, io_time, weighted_io_time;
            if (iss >> device >> device >> device >> read_io >> read_merges >> read_sectors >> read_time >> write_io >> write_merges >> write_sectors >> write_time >> io_in_progress >> io_time >> weighted_io_time) {
                if (device == disk) {
                    return io_time;
                }
            }
        }

        throw std::runtime_error("Disk " + disk + " not found in /proc/diskstats");
    };

    long initial_io_time = readDiskStats();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    long final_io_time = readDiskStats();

    return static_cast<double>(final_io_time - initial_io_time) / 10.0; // Return utilization as a percentage
}

double SystemMetrics::getDiskReadThroughput(const std::string& disk) {
    long sector_size = getSectorSize();
    auto [initial_read_sectors, initial_write_sectors] = readDiskStats(disk);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    auto [final_read_sectors, final_write_sectors] = readDiskStats(disk);

    long sectors_read = final_read_sectors - initial_read_sectors;
    double read_throughput = (sectors_read * sector_size) / 1024.0; // KB/s

    return read_throughput;
}

double SystemMetrics::getDiskWriteThroughput(const std::string& disk) {
    long sector_size = getSectorSize();
    auto [initial_read_sectors, initial_write_sectors] = readDiskStats(disk);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    auto [final_read_sectors, final_write_sectors] = readDiskStats(disk);

    long sectors_written = final_write_sectors - initial_write_sectors;
    double write_throughput = (sectors_written * sector_size) / 1024.0; // KB/s

    return write_throughput;
}

double SystemMetrics::getCpuStealTime() {
    return static_cast<double>(helpers::readCpuStealTime());
}

double SystemMetrics::getCpuTemperature() {
    return helpers::readCpuTemperature();
}

double SystemMetrics::getMemoryUtilization() {
    struct sysinfo memInfo;
    sysinfo(&memInfo);
    long long totalMemory = memInfo.totalram;
    long long freeMemory = memInfo.freeram;

    totalMemory *= memInfo.mem_unit;
    freeMemory *= memInfo.mem_unit;

    return 100.0 * (1.0 - static_cast<double>(freeMemory) / totalMemory);
}

double SystemMetrics::getAvailableMemory() {
    struct sysinfo memInfo;
    sysinfo(&memInfo);
    long long freeMemory = memInfo.freeram;
    freeMemory *= memInfo.mem_unit;
    return static_cast<double>(freeMemory) / (1024.0 * 1024.0); // Return MB
}

double SystemMetrics::getSwapUsage() {
    struct sysinfo memInfo;
    sysinfo(&memInfo);
    long long totalSwap = memInfo.totalswap;
    long long freeSwap = memInfo.freeswap;

    totalSwap *= memInfo.mem_unit;
    freeSwap *= memInfo.mem_unit;

    return 100.0 * (1.0 - static_cast<double>(freeSwap) / totalSwap);
}

double SystemMetrics::getMemoryPageFaults() {
    std::ifstream vmstatFile("/proc/vmstat");
    if (!vmstatFile.is_open()) {
        throw std::runtime_error("Failed to open /proc/vmstat");
    }

    std::string line;
    long pgfault = 0;
    long pgmajfault = 0;

    while (std::getline(vmstatFile, line)) {
        std::istringstream iss(line);
        std::string key;
        long value;
        if (iss >> key >> value) {
            if (key == "pgfault") {
                pgfault = value;
            } else if (key == "pgmajfault") {
                pgmajfault = value;
            }
        }
    }

    return static_cast<double>(pgfault + pgmajfault);
}

double SystemMetrics::getMemoryPageFaults() {
    std::ifstream vmstatFile("/proc/vmstat");
    if (!vmstatFile.is_open()) {
        throw std::runtime_error("Failed to open /proc/vmstat");
    }

    std::string line;
    long pgfault = 0;
    long pgmajfault = 0;

    while (std::getline(vmstatFile, line)) {
        std::istringstream iss(line);
        std::string key;
        long value;
        if (iss >> key >> value) {
            if (key == "pgfault") {
                pgfault = value;
            } else if (key == "pgmajfault") {
                pgmajfault = value;
            }
        }
    }

    return static_cast<double>(pgfault + pgmajfault);
}

double SystemMetrics::getDiskLatency(const std::string& disk) {
    auto [initial_io_time, initial_io_count] = readDiskStats(disk);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    auto [final_io_time, final_io_count] = readDiskStats(disk);

    long delta_io_time = final_io_time - initial_io_time;
    long delta_io_count = final_io_count - initial_io_count;

    if (delta_io_count == 0) {
        return 0.0; // Avoid division by zero if no I/O operations occurred
    }

    double latency = static_cast<double>(delta_io_time) / delta_io_count; // Time per I/O operation

    return latency;
}

double SystemMetrics::getDiskSpaceUtilization(const std::string& path) {
    struct statvfs stat;
    if (statvfs(path.c_str(), &stat) != 0) {
        throw std::runtime_error("Failed to get disk statistics");
    }

    double totalSpace = static_cast<double>(stat.f_blocks * stat.f_frsize);
    double freeSpace = static_cast<double>(stat.f_bfree * stat.f_frsize);

    return 100.0 * (1.0 - freeSpace / totalSpace);
}



double SystemMetrics::getNetworkBandwidthUtilization(const std::string& interface) {
    auto [initial_rx_bytes, initial_tx_bytes] = readNetworkStats(interface);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    auto [final_rx_bytes, final_tx_bytes] = readNetworkStats(interface);

    long rx_bytes_diff = final_rx_bytes - initial_rx_bytes;
    long tx_bytes_diff = final_tx_bytes - initial_tx_bytes;

    double bandwidth_utilization = static_cast<double>(rx_bytes_diff + tx_bytes_diff) / 1024.0; // Convert to KB/s

    return bandwidth_utilization;
}

double SystemMetrics::getNetworkLatency(const std::string& interface) {
    try {
        // Ping a known reliable external server
        return ping("8.8.8.8"); // Google's public DNS server
    } catch (const std::runtime_error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 0.0;
    }
}

double SystemMetrics::getNetworkErrors(const std::string& interface) {
    auto [rx_bytes, rx_packets, rx_errs, rx_drop, tx_errs, tx_drop] = helpers::readNetworkStats(interface);
    return static_cast<double>(rx_errs + tx_errs);
}

double SystemMetrics::getPacketLoss(const std::string& interface) {
    auto [rx_bytes, rx_packets, rx_errs, rx_drop, tx_errs, tx_drop] = helpers::readNetworkStats(interface);
    return static_cast<double>(rx_drop + tx_drop);
}

int SystemMetrics::getActiveConnections(const std::string& interface) {
    std::ifstream file("/proc/net/tcp");
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open /proc/net/tcp");
    }

    int active_connections = 0;
    std::string line;
    std::getline(file, line); // Skip the header line
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string local_address;
        iss >> local_address;
        if (local_address.find(interface) != std::string::npos) {
            active_connections++;
        }
    }

    return active_connections;
}

double SystemMetrics::getApplicationResponseTime(const std::string& app) {
    CURL* curl;
    CURLcode res;
    std::string readBuffer;

    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, app.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, helpers::WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

        auto start = std::chrono::high_resolution_clock::now();
        res = curl_easy_perform(curl);
        auto end = std::chrono::high_resolution_clock::now();

        if(res != CURLE_OK) {
            curl_easy_cleanup(curl);
            throw std::runtime_error("Failed to get application response time");
        }

        curl_easy_cleanup(curl);

        std::chrono::duration<double> duration = end - start;
        return duration.count();
    }

    throw std::runtime_error("Failed to initialize curl");
}

double SystemMetrics::getApplicationErrorRate(const std::string& app) {
    CURL* curl;
    CURLcode res;
    int numRequests = 100; // Number of requests to measure the error rate
    int numErrors = 0;
    std::string readBuffer;

    curl = curl_easy_init();
    if(curl) {
        for (int i = 0; i < numRequests; ++i) {
            curl_easy_setopt(curl, CURLOPT_URL, app.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, helpers::WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

            res = curl_easy_perform(curl);
            if(res != CURLE_OK) {
                numErrors++;
            }
        }

        curl_easy_cleanup(curl);
        return static_cast<double>(numErrors) / numRequests;
    }

    throw std::runtime_error("Failed to initialize curl");
}

double SystemMetrics::getRequestRate(const std::string& app) {
    CURL* curl;
    CURLcode res;
    int numRequests = 0;
    std::string readBuffer;

    auto start = std::chrono::high_resolution_clock::now();
    auto end = start + std::chrono::seconds(10); // Measure requests over 10 seconds

    curl = curl_easy_init();
    if(curl) {
        while (std::chrono::high_resolution_clock::now() < end) {
            curl_easy_setopt(curl, CURLOPT_URL, app.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, helpers::WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

            res = curl_easy_perform(curl);
            if(res == CURLE_OK) {
                numRequests++;
            }
        }

        curl_easy_cleanup(curl);

        std::chrono::duration<double> duration = end - start;
        return numRequests / duration.count(); // Requests per second
    }

    throw std::runtime_error("Failed to initialize curl");
}

double SystemMetrics::getThroughput(const std::string& app) {
    CURL* curl;
    CURLcode res;
    double totalBytes = 0;
    std::string readBuffer;

    auto start = std::chrono::high_resolution_clock::now();
    auto end = start + std::chrono::seconds(10); // Measure throughput over 10 seconds

    curl = curl_easy_init();
    if(curl) {
        while (std::chrono::high_resolution_clock::now() < end) {
            readBuffer.clear();
            curl_easy_setopt(curl, CURLOPT_URL, app.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, helpers::WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

            res = curl_easy_perform(curl);
            if(res == CURLE_OK) {
                totalBytes += readBuffer.size();
            }
        }

        curl_easy_cleanup(curl);

        std::chrono::duration<double> duration = end - start;
        return (totalBytes / duration.count()) * 8 / 1024 / 1024; // Convert to Mbps (Megabits per second)
    }

    throw std::runtime_error("Failed to initialize curl");
}

std::string SystemMetrics::detectDatabaseType(const std::string& db) {
    // Implement logic to detect database type based on connection string or configuration
    // Placeholder example logic
    if (db.find("postgresql://") == 0) {
        return "PostgreSQL";
    } else if (db.find("mongodb://") == 0) {
        return "MongoDB";
    } else if (db.find("aerospike://") == 0) {
        return "AerospikeDB";
    } else if (db.find("scylla://") == 0) {
        return "ScyllaDB";
    }
    return "Unknown";
}

double SystemMetrics::getQueryPerformance(const std::string& db) {
    std::string dbType = detectDatabaseType(db);

    if (dbType == "PostgreSQL") {
        PGconn* conn = PQconnectdb(db.c_str());
        if (PQstatus(conn) != CONNECTION_OK) {
            PQfinish(conn);
            throw std::runtime_error("Failed to connect to PostgreSQL");
        }

        auto start = std::chrono::high_resolution_clock::now();
        PGresult* res = PQexec(conn, "SELECT 1");
        auto end = std::chrono::high_resolution_clock::now();

        PQclear(res);
        PQfinish(conn);

        std::chrono::duration<double> duration = end - start;
        return duration.count();

    } else if (dbType == "MongoDB") {
        mongoc_client_t* client;
        mongoc_collection_t* collection;
        bson_t* query;
        mongoc_cursor_t* cursor;
        bson_error_t error;

        mongoc_init();
        client = mongoc_client_new(db.c_str());
        if (!client) {
            throw std::runtime_error("Failed to connect to MongoDB");
        }

        collection = mongoc_client_get_collection(client, "test", "collection");
        query = bson_new();

        auto start = std::chrono::high_resolution_clock::now();
        cursor = mongoc_collection_find_with_opts(collection, query, NULL, NULL);
        auto end = std::chrono::high_resolution_clock::now();

        if (mongoc_cursor_error(cursor, &error)) {
            mongoc_cursor_destroy(cursor);
            mongoc_collection_destroy(collection);
            mongoc_client_destroy(client);
            mongoc_cleanup();
            throw std::runtime_error("Failed to execute query on MongoDB");
        }

        mongoc_cursor_destroy(cursor);
        mongoc_collection_destroy(collection);
        mongoc_client_destroy(client);
        mongoc_cleanup();

        std::chrono::duration<double> duration = end - start;
        return duration.count();

    } else if (dbType == "AerospikeDB") {
        aerospike as;
        as_config config;
        as_error err;

        as_config_init(&config);
        if (!as_config_add_hosts(&config, db.c_str(), 3000)) {
            throw std::runtime_error("Failed to add AerospikeDB host");
        }

        aerospike_init(&as, &config);
        if (aerospike_connect(&as, &err) != AEROSPIKE_OK) {
            aerospike_destroy(&as);
            throw std::runtime_error("Failed to connect to AerospikeDB");
        }

        as_record* rec = NULL;
        as_key key;
        as_key_init_str(&key, "test", "demo", "key1");

        auto start = std::chrono::high_resolution_clock::now();
        if (aerospike_key_get(&as, &err, NULL, &key, &rec) != AEROSPIKE_OK) {
            aerospike_destroy(&as);
            throw std::runtime_error("Failed to execute query on AerospikeDB");
        }
        auto end = std::chrono::high_resolution_clock::now();

        as_record_destroy(rec);
        aerospike_destroy(&as);

        std::chrono::duration<double> duration = end - start;
        return duration.count();

    } else if (dbType == "ScyllaDB") {
        ScyllaDBClient client;
        if (!client.connect(db)) {
            throw std::runtime_error("Failed to connect to ScyllaDB");
        }

        auto start = std::chrono::high_resolution_clock::now();
        if (!client.executeQuery("SELECT now() FROM system.local")) {
            throw std::runtime_error("Failed to execute query on ScyllaDB");
        }
        auto end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double> duration = end - start;
        return duration.count();
    }

    throw std::runtime_error("Unknown database type");
}

double SystemMetrics::getConnectionPoolUtilization(const std::string& db) {
    std::string dbType = helpers::detectDatabaseType(db);

    if (dbType == "PostgreSQL") {
        return helpers::getPostgresConnectionPoolUtilization(db);
    } else if (dbType == "MongoDB") {
        return helpers::getMongoDBConnectionPoolUtilization(db);
    } else if (dbType == "AerospikeDB") {
        return helpers::getAerospikeDBConnectionPoolUtilization(db);
    } else if (dbType == "ScyllaDB") {
        return helpers::getScyllaDBConnectionPoolUtilization(db);
    }

    throw std::runtime_error("Unknown database type");
}

double SystemMetrics::getCacheHitMissRate(const std::string& db) {
    std::string dbType = helpers::detectDatabaseType(db);

    if (dbType == "PostgreSQL") {
        return helpers::getPostgresCacheHitMissRate(db);
    } else if (dbType == "MongoDB") {
        return helpers::getMongoDBCacheHitMissRate(db);
    } else if (dbType == "AerospikeDB") {
        return helpers::getAerospikeDBCacheHitMissRate(db);
    } else if (dbType == "ScyllaDB") {
        return helpers::getScyllaDBCacheHitMissRate(db);
    }

    throw std::runtime_error("Unknown database type");
}

double SystemMetrics::getTransactionRate(const std::string& db) {
    std::string dbType = helpers::detectDatabaseType(db);

    if (dbType == "PostgreSQL") {
        return helpers::getPostgresTransactionRate(db);
    } else if (dbType == "MongoDB") {
        return helpers::getMongoDBTransactionRate(db);
    } else if (dbType == "AerospikeDB") {
        return helpers::getAerospikeDBTransactionRate(db);
    } else if (dbType == "ScyllaDB") {
        return helpers::getScyllaDBTransactionRate(db);
    }

    throw std::runtime_error("Unknown database type");
}

int SystemMetrics::getFailedLoginAttempts() {
    return helpers::getFailedLoginAttempts();
}

int SystemMetrics::getIntrusionDetectionAlerts() {
    const std::vector<std::string> logFiles = {
        "/var/log/snort/alert",  // Snort
        "/var/log/suricata/alerts.log",  // Suricata
        "/var/ossec/logs/alerts/alerts.log"  // OSSEC
    };

    for (const auto& logFile : logFiles) {
        try {
            return helpers::getIntrusionDetectionAlerts(logFile);
        } catch (const std::runtime_error&) {
            // Continue to the next log file if the current one fails
        }
    }

    throw std::runtime_error("Failed to read any intrusion detection log files");
}

int SystemMetrics::getFirewallLogEntries() {
    const std::vector<std::string> logFiles = {
        "/var/log/ufw.log",  // UFW
        "/var/log/firewalld",  // Firewalld
        "/var/log/syslog",  // General syslog
        "/var/log/messages"  // General messages log
    };

    for (const auto& logFile : logFiles) {
        try {
            return helpers::getFirewallLogEntries(logFile);
        } catch (const std::runtime_error&) {
            // Continue to the next log file if the current one fails
        }
    }

    throw std::runtime_error("Failed to read any firewall log files");
}

int SystemMetrics::getVulnerabilityScans() {
    const std::vector<std::string> logFiles = {
        "/var/log/openvas/openvas.log",  // OpenVAS
        "/var/log/nessus/nessus.log",  // Nessus
        "/var/log/qualys/qualys.log"  // Qualys
    };

    for (const auto& logFile : logFiles) {
        try {
            return helpers::getVulnerabilityScans(logFile);
        } catch (const std::runtime_error&) {
            // Continue to the next log file if the current one fails
        }
    }

    throw std::runtime_error("Failed to read any vulnerability scan log files");
}

double SystemMetrics::getInstanceTypeUtilization() {
    const std::vector<std::string> logFiles = {
        "/var/log/cloud/instance_utilization.log",  // Example log file for instance utilization
        "/var/log/aws/instance_utilization.log",  // AWS example
        "/var/log/azure/instance_utilization.log"  // Azure example
    };

    for (const auto& logFile : logFiles) {
        try {
            return helpers::getInstanceTypeUtilization(logFile);
        } catch (const std::runtime_error&) {
            // Continue to the next log file if the current one fails
        }
    }

    throw std::runtime_error("Failed to read any instance type utilization log files");
}

double SystemMetrics::getAutoScalingMetrics() {
    const std::vector<std::string> logFiles = {
        "/var/log/cloud/auto_scaling.log",  // Example log file for auto-scaling metrics
        "/var/log/aws/auto_scaling.log",  // AWS example
        "/var/log/azure/auto_scaling.log"  // Azure example
    };

    for (const auto& logFile : logFiles) {
        try {
            return helpers::getAutoScalingMetrics(logFile);
        } catch (const std::runtime_error&) {
            // Continue to the next log file if the current one fails
        }
    }

    throw std::runtime_error("Failed to read any auto-scaling metrics log files");
}

double SystemMetrics::getResourceReservations() {
    const std::vector<std::string> logFiles = {
        "/var/log/cloud/resource_reservations.log",  // Example log file for resource reservations
        "/var/log/aws/resource_reservations.log",  // AWS example
        "/var/log/azure/resource_reservations.log"  // Azure example
    };

    for (const auto& logFile : logFiles) {
        try {
            return helpers::getResourceReservations(logFile);
        } catch (const std::runtime_error&) {
            // Continue to the next log file if the current one fails
        }
    }

    throw std::runtime_error("Failed to read any resource reservations log files");
}

double SystemMetrics::getPowerUsage() {
    const std::vector<std::string> logFiles = {
        "/var/log/cloud/power_usage.log",  // Example log file for power usage
        "/var/log/aws/power_usage.log",  // AWS example
        "/var/log/azure/power_usage.log"  // Azure example
    };

    for (const auto& logFile : logFiles) {
        try {
            return helpers::getPowerUsage(logFile);
        } catch (const std::runtime_error&) {
            // Continue to the next log file if the current one fails
        }
    }

    throw std::runtime_error("Failed to read any power usage log files");
}

double SystemMetrics::getEnergyEfficiency() {
    const std::vector<std::string> logFiles = {
        "/var/log/cloud/energy_efficiency.log",  // Example log file for energy efficiency
        "/var/log/aws/energy_efficiency.log",  // AWS example
        "/var/log/azure/energy_efficiency.log"  // Azure example
    };

    for (const auto& logFile : logFiles) {
        try {
            return helpers::getEnergyEfficiency(logFile);
        } catch (const std::runtime_error&) {
            // Continue to the next log file if the current one fails
        }
    }

    throw std::runtime_error("Failed to read any energy efficiency log files");
}

// Helper functions
double SystemMetrics::readStatFile() {
    // Example implementation to read /proc/stat file for CPU metrics
    std::ifstream statFile("/proc/stat");
    std::string line;
    if (std::getline(statFile, line)) {
        std::istringstream iss(line);
        std::string cpu;
        long user, nice, system, idle, iowait, irq, softirq, steal;
        iss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;
        return static_cast<double>(user + nice + system) / (user + nice + system + idle + iowait + irq + softirq + steal) * 100.0;
    }
    return 0.0;
}

double SystemMetrics::readProcFile(const std::string& path) {
    // Example implementation to read generic /proc file
    std::ifstream file(path);
    std::string line;
    if (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string key;
        long value;
        iss >> key >> value;
        return static_cast<double>(value);
    }
    return 0.0;
}

std::map<std::string, double> SystemMetrics::readNetDevFile() {
    // Example implementation to read /proc/net/dev file for network metrics
    std::map<std::string, double> metrics;
    std::ifstream netDevFile("/proc/net/dev");
    std::string line;
    while (std::getline(netDevFile, line)) {
        std::istringstream iss(line);
        std::string interface;
        long rxBytes, rxPackets, rxErrs, rxDrop, rxFifo, rxFrame, txBytes, txPackets, txErrs, txDrop, txFifo, txColls, txCarrier, txCompressed;
        iss >> interface >> rxBytes >> rxPackets >> rxErrs >> rxDrop >> rxFifo >> rxFrame >> txBytes >> txPackets >> txErrs >> txDrop >> txFifo >> txColls >> txCarrier >> txCompressed;
        metrics[interface] = static_cast<double>(rxBytes + txBytes);
    }
    return metrics;
}

bool SystemMetrics::shouldScale(SystemMetrics& sysMetrics, const std::string& instance, const std::string& app, const std::string& disk, const std::string& interface) {
    double cpuUtilization = sysMetrics.getCpuUtilization();
    double memoryUtilization = sysMetrics.getMemoryUtilization();
    double diskIoUtilization = sysMetrics.getDiskIoUtilization(disk);
    double networkUtilization = sysMetrics.getNetworkBandwidthUtilization(interface);
    double autoScalingMetrics = sysMetrics.getAutoScalingMetrics();
    double appResponseTime = sysMetrics.getApplicationResponseTime(app);
    double appErrorRate = sysMetrics.getApplicationErrorRate(app);
    double requestRate = sysMetrics.getRequestRate(app);
    double gpuUsage = sysMetrics.getGpuUsage();
    double gpuTemperature = sysMetrics.getGpuTemperature();

    const double CPU_THRESHOLD = 80.0; // in percent
    const double MEMORY_THRESHOLD = 80.0; // in percent
    const double DISK_IO_THRESHOLD = 80.0; // in percent
    const double NETWORK_THRESHOLD = 80.0; // in percent
    const double RESPONSE_TIME_THRESHOLD = 2.0; // in seconds
    const double ERROR_RATE_THRESHOLD = 0.05; // 5 percent
    const double GPU_USAGE_THRESHOLD = 80.0; // in percent
    const double GPU_TEMPERATURE_THRESHOLD = 85.0; // in degrees Celsius

    if (cpuUtilization > CPU_THRESHOLD || memoryUtilization > MEMORY_THRESHOLD || 
        diskIoUtilization > DISK_IO_THRESHOLD || networkUtilization > NETWORK_THRESHOLD ||
        appResponseTime > RESPONSE_TIME_THRESHOLD || appErrorRate > ERROR_RATE_THRESHOLD ||
        gpuUsage > GPU_USAGE_THRESHOLD || gpuTemperature > GPU_TEMPERATURE_THRESHOLD ||
        autoScalingMetrics > someDefinedThreshold) {
        return true; // Indicate that scaling is needed
    }
    return false;
}

std::string SystemMetrics::getIpAddress() {
    char hostname[1024];
    hostname[1023] = '\0';

    // Get the hostname
    if (gethostname(hostname, 1023) != 0) {
        throw std::runtime_error("Failed to get hostname");
    }

    // Get the host information
    struct addrinfo hints, *info, *p;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; // AF_INET for IPv4, AF_INET6 for IPv6
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_CANONNAME;

    if (getaddrinfo(hostname, "http", &hints, &info) != 0) {
        throw std::runtime_error("Failed to get address info");
    }

    // Loop through the results and get the first IP address
    for (p = info; p != nullptr; p = p->ai_next) {
        struct sockaddr_in *addr = reinterpret_cast<struct sockaddr_in *>(p->ai_addr);
        std::string ip = inet_ntoa(addr->sin_addr);
        freeaddrinfo(info);
        return ip;
    }

    freeaddrinfo(info);
    throw std::runtime_error("Failed to get IP address");
}

/**
 * Begin NVIDIA GPU Functions
 */


bool SystemMetrics::hasNvidiaGpu() {
    // Run the nvidia-smi command and check the output
    std::array<char, 128> buffer;
    std::string result;
    std::shared_ptr<FILE> pipe(popen("nvidia-smi -L", "r"), pclose);

    if (!pipe) {
        throw std::runtime_error("Failed to run nvidia-smi command");
    }

    while (fgets(buffer.data(), 128, pipe.get()) != nullptr) {
        result += buffer.data();
    }

    // Check if the output contains information about any NVIDIA GPUs
    if (result.find("GPU") != std::string::npos) {
        return true;
    }

    return false;
}

void SystemMetrics::initializeNVML() {
    nvmlReturn_t result = nvmlInit();
    if (NVML_SUCCESS != result) {
        throw std::runtime_error("Failed to initialize NVML: " + std::string(nvmlErrorString(result)));
    }
}

void SystemMetrics::shutdownNVML() {
    nvmlReturn_t result = nvmlShutdown();
    if (NVML_SUCCESS != result) {
        std::cerr << "Failed to shutdown NVML: " << nvmlErrorString(result) << std::endl;
    }
}

float SystemMetrics::getGpuTemperature(unsigned int gpuIndex) {
    nvmlDevice_t device;
    nvmlReturn_t result = nvmlDeviceGetHandleByIndex(gpuIndex, &device);
    if (NVML_SUCCESS != result) {
        throw std::runtime_error("Failed to get device handle: " + std::string(nvmlErrorString(result)));
    }

    unsigned int temp;
    result = nvmlDeviceGetTemperature(device, NVML_TEMPERATURE_GPU, &temp);
    if (NVML_SUCCESS != result) {
        throw std::runtime_error("Failed to get temperature: " + std::string(nvmlErrorString(result)));
    }

    return static_cast<float>(temp);
}

float SystemMetrics::getGpuUsage(unsigned int gpuIndex) {
    nvmlDevice_t device;
    nvmlReturn_t result = nvmlDeviceGetHandleByIndex(gpuIndex, &device);
    if (NVML_SUCCESS != result) {
        throw std::runtime_error("Failed to get device handle: " + std::string(nvmlErrorString(result)));
    }

    nvmlUtilization_t utilization;
    result = nvmlDeviceGetUtilizationRates(device, &utilization);
    if (NVML_SUCCESS != result) {
        throw std::runtime_error("Failed to get utilization: " + std::string(nvmlErrorString(result)));
    }

    return static_cast<float>(utilization.gpu);
}

float SystemMetrics::getGpuMemoryUsage(unsigned int gpuIndex) {
    nvmlDevice_t device;
    nvmlReturn_t result = nvmlDeviceGetHandleByIndex(gpuIndex, &device);
    if (NVML_SUCCESS != result) {
        throw std::runtime_error("Failed to get device handle: " + std::string(nvmlErrorString(result)));
    }

    nvmlMemory_t memory;
    result = nvmlDeviceGetMemoryInfo(device, &memory);
    if (NVML_SUCCESS != result) {
        throw std::runtime_error("Failed to get memory info: " + std::string(nvmlErrorString(result)));
    }

    return static_cast<float>(memory.used) / static_cast<float>(memory.total) * 100.0f;
}

float SystemMetrics::getGpuPowerUsage(unsigned int gpuIndex) {
    nvmlDevice_t device;
    nvmlReturn_t result = nvmlDeviceGetHandleByIndex(gpuIndex, &device);
    if (NVML_SUCCESS != result) {
        throw std::runtime_error("Failed to get device handle: " + std::string(nvmlErrorString(result)));
    }

    unsigned int power;
    result = nvmlDeviceGetPowerUsage(device, &power);
    if (NVML_SUCCESS != result) {
        throw std::runtime_error("Failed to get power usage: " + std::string(nvmlErrorString(result)));
    }

    return static_cast<float>(power) / 1000.0f; // Convert to watts
}

float NvidiaGPUInfo::getGpuFanSpeed(unsigned int gpuIndex) {
    nvmlDevice_t device;
    nvmlReturn_t result = nvmlDeviceGetHandleByIndex(gpuIndex, &device);
    if (NVML_SUCCESS != result) {
        throw std::runtime_error("Failed to get device handle: " + std::string(nvmlErrorString(result)));
    }

    unsigned int speed;
    result = nvmlDeviceGetFanSpeed(device, &speed);
    if (NVML_SUCCESS != result) {
        throw std::runtime_error("Failed to get fan speed: " + std::string(nvmlErrorString(result)));
    }

    return static_cast<float>(speed);
}

float SystemMetrics::getGpuCoreClock(unsigned int gpuIndex) {
    nvmlDevice_t device;
    nvmlReturn_t result = nvmlDeviceGetHandleByIndex(gpuIndex, &device);
    if (NVML_SUCCESS != result) {
        throw std::runtime_error("Failed to get device handle: " + std::string(nvmlErrorString(result)));
    }

    unsigned int clock;
    result = nvmlDeviceGetClock(device, NVML_CLOCK_SM, NVML_CLOCK_ID_CURRENT, &clock);
    if (NVML_SUCCESS != result) {
        throw std::runtime_error("Failed to get core clock: " + std::string(nvmlErrorString(result)));
    }

    return static_cast<float>(clock);
}

float SystemMetrics::getGpuMemoryClock(unsigned int gpuIndex) {
    nvmlDevice_t device;
    nvmlReturn_t result = nvmlDeviceGetHandleByIndex(gpuIndex, &device);
    if (NVML_SUCCESS != result) {
        throw std::runtime_error("Failed to get device handle: " + std::string(nvmlErrorString(result)));
    }

    unsigned int clock;
    result = nvmlDeviceGetClock(device, NVML_CLOCK_MEM, NVML_CLOCK_ID_CURRENT, &clock);
    if (NVML_SUCCESS != result) {
        throw std::runtime_error("Failed to get memory clock: " + std::string(nvmlErrorString(result)));
    }

    return static_cast<float>(clock);
}

float SystemMetrics::getGpuMemoryBandwidthUsage(unsigned int gpuIndex) {
    nvmlDevice_t device;
    nvmlReturn_t result = nvmlDeviceGetHandleByIndex(gpuIndex, &device);
    if (NVML_SUCCESS != result) {
        throw std::runtime_error("Failed to get device handle: " + std::string(nvmlErrorString(result)));
    }

    nvmlMemory_t memory;
    result = nvmlDeviceGetMemoryInfo(device, &memory);
    if (NVML_SUCCESS != result) {
        throw std::runtime_error("Failed to get memory info: " + std::string(nvmlErrorString(result)));
    }

    return static_cast<float>(memory.usedBandwidth);
}

std::string SystemMetrics::getGpuComputeMode(unsigned int gpuIndex) {
    nvmlDevice_t device;
    nvmlReturn_t result = nvmlDeviceGetHandleByIndex(gpuIndex, &device);
    if (NVML_SUCCESS != result) {
        throw std::runtime_error("Failed to get device handle: " + std::string(nvmlErrorString(result)));
    }

    nvmlComputeMode_t computeMode;
    result = nvmlDeviceGetComputeMode(device, &computeMode);
    if (NVML_SUCCESS != result) {
        throw std::runtime_error("Failed to get compute mode: " + std::string(nvmlErrorString(result)));
    }

    switch (computeMode) {
        case NVML_COMPUTEMODE_DEFAULT:
            return "Default";
        case NVML_COMPUTEMODE_EXCLUSIVE_THREAD:
            return "Exclusive Thread";
        case NVML_COMPUTEMODE_PROHIBITED:
            return "Prohibited";
        case NVML_COMPUTEMODE_EXCLUSIVE_PROCESS:
            return "Exclusive Process";
        default:
            return "Unknown";
    }
}

float SystemMetrics::getGpuPCIeThroughput(unsigned int gpuIndex, nvmlPcieUtilCounter_t counter) {
    nvmlDevice_t device;
    nvmlReturn_t result = nvmlDeviceGetHandleByIndex(gpuIndex, &device);
    if (NVML_SUCCESS != result) {
        throw std::runtime_error("Failed to get device handle: " + std::string(nvmlErrorString(result)));
    }

    unsigned int throughput;
    result = nvmlDeviceGetPcieThroughput(device, counter, &throughput);
    if (NVML_SUCCESS != result) {
        throw std::runtime_error("Failed to get PCIe throughput: " + std::string(nvmlErrorString(result)));
    }

    return static_cast<float>(throughput);
}
/**
 * end NIVIDIA GPU Functions here:
 */

bool SystemMetrics::isTbbAvailable() {
    try {
        // Simple TBB task to check availability
        tbb::parallel_invoke([]{});
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

bool SystemMetrics::isMpiAvailable() {
    int flag;
    MPI_Initialized(&flag);
    if (!flag) {
        int argc = 0;
        char **argv = nullptr;
        MPI_Init(&argc, &argv);
    }
    MPI_Initialized(&flag);
    if (flag) {
        MPI_Finalize();
        return true;
    }
    return false;
}

std::vector<SystemMetrics::ProcessMemoryInfo> SystemMetrics::getProcessMemoryUsage(const std::vector<pid_t>& pids) {
    std::vector<ProcessMemoryInfo> processMemoryInfos;

    for (pid_t pid : pids) {
        std::ifstream statusFile("/proc/" + std::to_string(pid) + "/status");
        if (!statusFile.is_open()) {
            throw std::runtime_error("Failed to open /proc/" + std::to_string(pid) + "/status");
        }

        std::string line;
        long rssMemory = 0; // Resident Set Size in KB
        std::string processName;

        while (std::getline(statusFile, line)) {
            if (line.find("Name:") == 0) {
                std::istringstream iss(line);
                std::string key;
                if (iss >> key >> processName) {
                    // Process name found
                }
            }

            if (line.find("VmRSS:") == 0) {
                std::istringstream iss(line);
                std::string key;
                long value;
                std::string unit;
                if (iss >> key >> value >> unit) {
                    rssMemory = value;
                    break;
                }
            }
        }

        statusFile.close();

        if (rssMemory > 0) {
            double memoryUsageMB = static_cast<double>(rssMemory) / 1024.0; // Convert to MB
            processMemoryInfos.push_back({processName, memoryUsageMB});
        }
    }

    return processMemoryInfos;
}