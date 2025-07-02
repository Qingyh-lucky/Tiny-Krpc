#include "KrpcBreakState.h"
#include <string>


void CircuitBreakerManager::RecordFailure(const std::string &method_key) {
        std::lock_guard<std::mutex> lock(mutex_);  // 🚩 必须加锁
        auto &cb = breakers_[method_key];
        cb.failure_count++;
        std::cout<<"失败次数："<<cb.failure_count<<std::endl;
        cb.last_failure_time = NowMs();

        if (cb.state == CircuitBreaker::HALF_OPEN) {
            // 探活失败，回到OPEN
            cb.state = CircuitBreaker::OPEN;
            cb.failure_count = 0;
            cb.success_count = 0;
            
            std::cout<< "[CB] HALF_OPEN探活失败，重新OPEN: " << method_key << std::endl;
        } else if (cb.failure_count >= cb.failure_threshold) {
            cb.state = CircuitBreaker::OPEN;
            // cb.last_failure_time = NowMs();
           std::cout<< "[CB] 熔断 OPEN triggered for: " << method_key << std::endl;
        }

        PrintStatus(method_key, cb);
    }


void CircuitBreakerManager::RecordSuccess(const std::string &method_key) {
    std::lock_guard<std::mutex> lock(mutex_);  // 🚩 必须加锁
    auto &cb = breakers_[method_key];
    if (cb.state == CircuitBreaker::HALF_OPEN) {
        cb.success_count++;
        if (cb.success_count >= cb.success_threshold) {
            cb.state = CircuitBreaker::CLOSED;
            cb.failure_count = 0;
            cb.success_count = 0;                
            std::cout<< "[CB] HALF_OPEN探活成功，恢复 CLOSED: " << method_key << std::endl;
        }
    } else {
        cb.failure_count = 0; // 正常调用成功，归零
    }

    PrintStatus(method_key, cb);
}

bool CircuitBreakerManager::ShouldSkip(const std::string &method_key) {
        std::lock_guard<std::mutex> lock(mutex_);  // 🚩 必须加锁
        auto &cb = breakers_[method_key];
        int64_t now = NowMs();

        if (cb.state == CircuitBreaker::OPEN) {
            if (now - cb.last_failure_time >= cb.recovery_timeout_ms) {
                cb.failure_count = 0;
                cb.success_count = 0;
                cb.state = CircuitBreaker::HALF_OPEN;          
                std::cout<< "[CB] Recovery timeout reached, switching to HALF_OPEN for " << method_key << std::endl ;
                return false; // half-open允许探活
            } else {
                
                std::cout<<"[CB] Node SKIPPED due to OPEN state: " << method_key<< std::endl;
                return true;
            }
        }

        return false; // CLOSED 或 HALF_OPEN 都允许调用
    }

void CircuitBreakerManager::PrintStatus(const std::string& method_key, const CircuitBreaker& cb) const {
    std::cout << "[CB] " << method_key
              << " => state: " << cb.state_str()
              << ", failures: " << cb.failure_count
              << ", successes: " << cb.success_count
              << std::endl;
}