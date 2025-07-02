#ifndef _Krpcprovider_H__
#define _Krpcprovider_H__
#include "google/protobuf/service.h"
#include "zookeeperutil.h"
#include<muduo/net/TcpServer.h>
#include<muduo/net/EventLoop.h>
#include<muduo/net/InetAddress.h>
#include<muduo/net/TcpConnection.h>
#include<google/protobuf/descriptor.h>
#include<functional>
#include<string>
#include<unordered_map>

class KrpcProvider
{
public:
    //这里是提供给外部使用的，可以发布rpc方法的函数接口。
    void NotifyService(google::protobuf::Service* service);
      ~KrpcProvider();
    //启动rpc服务节点，开始提供rpc远程网络调用服务
    void Run();
private:
    muduo::net::EventLoop event_loop;
    /*
    service_name => service *记录服务对象
                        {
                        MethodDescriptor* ==>login、register等多种函数
                        }

    例子，在user.proto中这里定义了一个方法
    service UserServiceRpc{
    rpc Login(LoginRequest) returns(LoginResponse);
    rpc Register(RegisterRequest) returns(RegisterResponse);
    }

    那么这里的ServiceInfo
    {
        google::protobuf::Service* service=UserServiceRpc;
        method_map{
        (Login,(google::protobuf::MethodDescriptor)&Login),
        (Register,(google::protobuf::MethodDescriptor)&Register),
        }
    }
        service_map
        {
        UserServiceRpc,
        ServiceInfo（上面的那个）
        }
    */
   //服务类型信息
    struct ServiceInfo
    {
        google::protobuf::Service* service;                                                      //一个具体的服务对象
        std::unordered_map<std::string, const google::protobuf::MethodDescriptor*> method_map;  //保存服务对象中方法名，保存服务对象中方法描述符的映射
    };
    //这里的第一个参数是服务方法的名字
    std::unordered_map<std::string, ServiceInfo>service_map;//保存服务对象和rpc方法
    
    void OnConnection(const muduo::net::TcpConnectionPtr& conn);
    void OnMessage(const muduo::net::TcpConnectionPtr& conn, muduo::net::Buffer* buffer, muduo::Timestamp receive_time);
    void SendRpcResponse(const muduo::net::TcpConnectionPtr& conn, google::protobuf::Message* response);
};
#endif 







