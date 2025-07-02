#ifndef _KrpcLoadBalancer_H
#define _KrpcLoadBalancer_H
class LoadBalancer {
public:
    static LoadBalancer& Instance() {
        static LoadBalancer instance;
        return instance;
    }

    uint64_t NextHealthyIndex(size_t size) {
        //std::cout<<healthy_counter_<<std::endl;
        return healthy_counter_.fetch_add(1) % size;
    }

    uint64_t NextHalfOpenIndex(size_t size) {
        //std::cout<<half_open_counter_<<std::endl;
        return half_open_counter_.fetch_add(1) % size;
    }
private:
    std::atomic<uint64_t> healthy_counter_{0};
    std::atomic<uint64_t> half_open_counter_{0};
    LoadBalancer() = default;
};
#endif