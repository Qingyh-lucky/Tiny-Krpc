#ifndef _KrpcRequestStats_H
#define _KrpcRequestStats_H
#include <mutex>
#include <unordered_map>
#include <string>

class GlobalRequestStats {
public:
    static GlobalRequestStats& Instance() {
        static GlobalRequestStats instance;
        return instance;
    }

    void AddRequest() {
        std::lock_guard<std::mutex> lock(mutex_);
        total_requests++;
    }

    void AddNodeCall(const std::string& node) {
        std::lock_guard<std::mutex> lock(mutex_);
        node_call_counts[node]++;
    }

    void AddSkippedRequest() {
        std::lock_guard<std::mutex> lock(mutex_);
        skipped_due_to_cb++;
    }

    void Print() {
        std::lock_guard<std::mutex> lock(mutex_);
        printf("\n=== Global Request Stats ===\n");
        printf("Total Requests: %d\n", total_requests);
        printf("Skipped due to circuit break: %d\n", skipped_due_to_cb);
        printf("--- Node Call Counts ---\n");
        for (const auto& kv : node_call_counts) {
            printf("%s: %d times\n", kv.first.c_str(), kv.second);
        }
    }
    void Clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        total_requests = 0;
        skipped_due_to_cb = 0;
        node_call_counts.clear();
    }


private:
    GlobalRequestStats() = default;

    int total_requests = 0;
    int skipped_due_to_cb = 0;
    std::unordered_map<std::string, int> node_call_counts;
    std::mutex mutex_;
};
#endif