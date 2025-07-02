#ifndef _KrpcNodeStats_H
#define _KrpcNodeStats_H
//用于实现节点的统计，服务器A在此次请求中被调用了多少次，服务B在此次请求中调用了多少次等
#include <unordered_map>
#include <string>
#include <mutex>
#include <iostream>

class NodeStats {
public:
    static NodeStats& Instance() {
        static NodeStats instance;
        return instance;
    }

    void RecordNode(const std::string& node) {
        std::lock_guard<std::mutex> lock(mutex_);
        counts_[node]++;
    }

    void PrintStats() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::cout << "\n=== Node Call Statistics ===\n";
        for (const auto& kv : counts_) {
            std::cout << kv.first << ": " << kv.second << " times\n";
        }
    }
    void clear()
    {
        counts_.clear();
    }

private:
    std::unordered_map<std::string, int> counts_;
    std::mutex mutex_;
};
#endif