#ifndef PERFORMANCE_MONITOR_H
#define PERFORMANCE_MONITOR_H

#include <vector>
#include <mutex>
#include <iostream>
#include <numeric>
#include <algorithm>

class PerformanceMonitor {
private:
    std::vector<long long> restApiLatencies;      // For REST API latencies
    std::vector<long long> websocketApiLatencies;  // For WebSocket API latencies
    std::mutex latencyMutex;

    // Memory usage tracking
    size_t peakMemoryUsage = 0;

    // Throughput tracking
    std::atomic<long long> orderCount{0}; // Total orders placed
    std::atomic<long long> marketDataCount{0}; // Total market data updates processed
    
    std::chrono::steady_clock::time_point startTime;

public:
    void recordRestApiLatency(long long microseconds) { // For REST API
        std::lock_guard<std::mutex> lock(latencyMutex);
        restApiLatencies.push_back(microseconds);
    }

    void recordWebSocketApiLatency(long long microseconds) { // For WebSocket API
        std::lock_guard<std::mutex> lock(latencyMutex);
        websocketApiLatencies.push_back(microseconds);
    }

    void recordMarketDataLatency(long long microseconds) { // Add this method
        std::lock_guard<std::mutex> lock(latencyMutex);
        websocketApiLatencies.push_back(microseconds); // Assuming you want to track it here
    }

    void checkMemoryUsage() {
        // This is platform-specific and simplified
        // For a real implementation, use platform-specific APIs
        #ifdef _WIN32
        // Windows implementation
        PROCESS_MEMORY_COUNTERS pmc;
        if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
            size_t currentUsage = pmc.WorkingSetSize;
            peakMemoryUsage = std::max(peakMemoryUsage, currentUsage);
        }
        #else
        // Linux implementation (simplified)
        FILE* file = fopen("/proc/self/status", "r");
        if (file) {
            char line[128];
            while (fgets(line, 128, file) != NULL) {
                if (strncmp(line, "VmRSS:", 6) == 0) {
                    size_t currentUsage = 0;
                    sscanf(line + 6, "%zu", &currentUsage);
                    currentUsage *= 1024; // Convert from KB to bytes
                    peakMemoryUsage = std::max(peakMemoryUsage, currentUsage);
                    break;
                }
            }
            fclose(file);
        }
        #endif
    }

    void printStatistics() {
        std::lock_guard<std::mutex> lock(latencyMutex);
        
        if (!restApiLatencies.empty()) {
            std::cout << "REST API Latency (microseconds):\n";
            printLatencyStats(restApiLatencies);
        }
        
        if (!websocketApiLatencies.empty()) {
            std::cout << "WebSocket API Latency (microseconds):\n";
            printLatencyStats(websocketApiLatencies);
        }

        std::cout << "Peak Memory Usage: " << (peakMemoryUsage / 1024.0 / 1024.0) << " MB\n";
    }


private:
    void printLatencyStats(const std::vector<long long>& latencies) {
        if (latencies.empty()) return;
        
        long long sum = std::accumulate(latencies.begin(), latencies.end(), 0LL);
        double mean = sum / static_cast<double>(latencies.size());
        
        std::vector<long long> sorted = latencies;
        std::sort(sorted.begin(), sorted.end());
        
        long long min = sorted.front();
        long long max = sorted.back();
        long long median = sorted[sorted.size() / 2];
        
        // Calculate 95th percentile
        size_t p95_index = static_cast<size_t>(sorted.size() * 0.95);
        long long p95 = sorted[p95_index];
        
        // Calculate 99th percentile
        size_t p99_index = static_cast<size_t>(sorted.size() * 0.99);
        long long p99 = sorted[p99_index];
        
        std::cout << "  Samples: " << latencies.size() << "\n";
        std::cout << "  Min: " << min << "\n";
        std::cout << "  Max: " << max << "\n";
        std::cout << "  Mean: " << mean << "\n";
        std::cout << "  Median: " << median << "\n";
        std::cout << "  95th Percentile: " << p95 << "\n";
        std::cout << "  99th Percentile: " << p99 << "\n";
    }
};

#endif // PERFORMANCE_MONITOR_H 