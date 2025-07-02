#include "KrpcDistributedCounter.h"
#include <iostream>

DistributedCounter::DistributedCounter(const std::string& redis_host, int redis_port)
    : redis_host_(redis_host), redis_port_(redis_port) {
    // 连接到 Redis 服务器
    redis_context_ = redisConnect(redis_host_.c_str(), redis_port_);
    if (redis_context_ == nullptr || redis_context_->err) {
        if (redis_context_) {
            std::cerr << "Connection error: " << redis_context_->errstr << std::endl;
        } else {
            std::cerr << "Connection error: can't allocate redis context" << std::endl;
        }
        exit(1);
    }
}

DistributedCounter::~DistributedCounter() {
    if (redis_context_) {
        redisFree(redis_context_);
    }
}

void DistributedCounter::Increment(const std::string& key) {
    redisReply* reply = (redisReply*)redisCommand(redis_context_, "INCR %s", key.c_str());
    if (reply) {
        std::cout << "Counter value after increment: " << reply->integer << std::endl;
        freeReplyObject(reply);
    } else {
        std::cerr << "Failed to increment counter" << std::endl;
    }
}

int DistributedCounter::GetValue(const std::string& key) {
    redisReply* reply = (redisReply*)redisCommand(redis_context_, "GET %s", key.c_str());
    if (reply) {
        int value = reply->integer;
        freeReplyObject(reply);
        return value;
    } else {
        std::cerr << "Failed to get counter value" << std::endl;
        return -1;
    }
}