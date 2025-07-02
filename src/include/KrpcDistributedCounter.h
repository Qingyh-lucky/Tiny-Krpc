#ifndef DISTRIBUTED_COUNTER_H
#define DISTRIBUTED_COUNTER_H

#include <string>
#include <hiredis/hiredis.h>

class DistributedCounter {
public:
    DistributedCounter(const std::string& redis_host, int redis_port);
    ~DistributedCounter();

    void Increment(const std::string& key);
    int GetValue(const std::string& key);

private:
    std::string redis_host_;
    int redis_port_;
    redisContext* redis_context_;
};

#endif