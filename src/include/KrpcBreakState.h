#ifndef _KrpBreakState_H
#define _KrpBreakState_H
#include <iostream>
#include <chrono>
#include <string>
#include <mutex>
#include <unordered_map>

struct CircuitBreaker {
    enum State { CLOSED, OPEN, HALF_OPEN };

    State state = CLOSED;
    int failure_count = 0;
    int failure_threshold = 5;     // 测试阈值：连续5次失败就熔断
    int success_count = 0;
    int success_threshold = 1;     // half-open 探活成功1次就恢复
    int64_t last_failure_time = 0;
    int recovery_timeout_ms = 200; // 熔断多少ms后转half-open

    std::string state_str() const {
        switch(state) {
            case CLOSED: return "CLOSED";
            case OPEN: return "OPEN";
            case HALF_OPEN: return "HALF_OPEN";
            default: return "UNKNOWN";
        }
    }
};


class CircuitBreakerManager {
public:
    static CircuitBreakerManager& GetInstance() {
        static CircuitBreakerManager instance;
        return instance;
    }

    CircuitBreakerManager(const CircuitBreakerManager&) = delete;
    CircuitBreakerManager& operator=(const CircuitBreakerManager&) = delete;

    void RecordFailure(const std::string &method_key) ;

    void RecordSuccess(const std::string &method_key) ;

    bool ShouldSkip(const std::string &method_key) ;

    

    CircuitBreaker& GetBreaker(const std::string &method_key) {
        std::lock_guard<std::mutex> lock(mutex_);
        return breakers_[method_key];
    }
    int64_t NowMs() const{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
    }
    
    void PrintStatus(const std::string &method_key, const CircuitBreaker &cb) const;
private:
    CircuitBreakerManager() =default;

    std::unordered_map<std::string, CircuitBreaker> breakers_;
    std::mutex mutex_;


};

#endif