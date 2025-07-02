#include "Krpcchannel.h"
#include "Krpcheader.pb.h"
#include "zookeeperutil.h"
#include "Krpcapplication.h"
#include "Krpccontroller.h"
#include "memory"
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "KrpcLogger.h"

std::mutex g_data_mutx;  // 全局互斥锁，用于保护共享数据的线程安全

/*
格式：
header_size+service_name+method_name args_size+args
*/
// RPC调用的核心方法，负责将客户端的请求序列化并发送到服务端，同时接收服务端的响应
void KrpcChannel::CallMethod(const ::google::protobuf::MethodDescriptor *method,
                             ::google::protobuf::RpcController *controller,
                             const ::google::protobuf::Message *request,
                             ::google::protobuf::Message *response,
                             ::google::protobuf::Closure *done)
{
    if (-1 == m_clientfd) {  // 如果客户端socket未初始化
        // 获取服务对象名和方法名
        const google::protobuf::ServiceDescriptor *sd = method->service();
        service_name = sd->name();  // 服务名
        method_name = method->name();  // 方法名

        // 客户端需要查询ZooKeeper，找到提供该服务的服务器地址
        ZkClient zkCli;
        zkCli.Start();  // 连接ZooKeeper服务器
        //std::string host_data = QueryServiceHost(&zkCli, service_name, method_name, m_idx);  // 查询服务地址,这个是单点的节点查询
        
        //遍历当前路径下的所有节点
        std::string service_method_path='/'+service_name+'/'+method_name;
        std::vector<std::string> children=zkCli.GetChildren(service_method_path);

        // --- 新增：分健康和探活 ---
        std::vector<std::string> healthy_nodes;
        std::vector<std::string> half_open_nodes;

        int64_t now = CircuitBreakerManager::GetInstance().NowMs();
        for (auto &child : children) {
            std::string node = child;
            std::string method_ipport = '/'+service_name + "/" + method_name + "/" + node;

            auto &cb = CircuitBreakerManager::GetInstance().GetBreaker(method_ipport);
            std::cout << "[CB Status] Node: " << node
            << " State: " << cb.state_str()
            << " FailCount: " << cb.failure_count
            << " LastFailure: " << cb.last_failure_time 
             << " recovery_timeout_ms: " << cb.recovery_timeout_ms 
            <<"method_ipport:"<<method_ipport<<std::endl;
            if (cb.state == CircuitBreaker::OPEN) {
                //int64_t now = CircuitBreakerManager::GetInstance().NowMs();
                if (now - cb.last_failure_time >= cb.recovery_timeout_ms) {
                    // 超时，尝试探活
                    std::cout<<"超时，尝试探活"<<std::endl;
                    cb.state = CircuitBreaker::HALF_OPEN;
                    half_open_nodes.push_back(node);
                } else {
                    // OPEN 且未超时，跳过
                    continue;
                }
            } else if (cb.state == CircuitBreaker::HALF_OPEN) {
                half_open_nodes.push_back(node);
            } else { // CLOSED
                healthy_nodes.push_back(node);
            }
        }


        // 打印候选
        std::cout << "Healthy nodes: ";
        for (auto& n : healthy_nodes) std::cout << n << " ";
        std::cout << std::endl;

        std::cout << "Half-open nodes: ";
        for (auto& n : half_open_nodes) std::cout << n << " ";
        std::cout << std::endl;

        //负载均衡
        std::string host_data;
        if (!healthy_nodes.empty()) {
            // 正常节点轮询或随机
            uint64_t index = LoadBalancer::Instance().NextHealthyIndex(healthy_nodes.size());
            host_data = healthy_nodes[index % healthy_nodes.size()];
            std::cout << "选中 Healthy 节点: " << host_data << std::endl;
        } else if (!half_open_nodes.empty()) {
            // 如果没有正常节点，只能探活
            // 注意：只选一个探活节点！
            // host_data = half_open_nodes[0]; // 也可以随机
            // std::cout << "选中 Half-Open 节点探活: " << host_data << std::endl;


            // 随机选一个可以探活的半开节点
            uint64_t index = LoadBalancer::Instance().NextHalfOpenIndex(half_open_nodes.size());
            host_data = half_open_nodes[index % half_open_nodes.size()];
            std::cout << "选中 Half-Open 节点探活: " << host_data << std::endl;
        } else {
            std::cout << "[CB] 全部熔断，无法提供服务！" << std::endl;
            controller->SetFailed("No available provider node due to circuit break.");
            return;
        }

        auto* my_controller = dynamic_cast<Krpccontroller*>(controller);
        if (my_controller) {
            my_controller->SetMethodIpPort(service_method_path+'/'+host_data);
        }

     
        // // 做负载均衡，随机选择一个
        // // srand(time(nullptr));
        // // int index = rand() % candidates.size();
        // // std::string host_data = candidates[index];  // host_data = "192.168.1.10:8000"

        GlobalRequestStats::Instance().AddNodeCall(host_data);


        size_t m_idx = host_data.find(":");
        if (m_idx == std::string::npos) {
            controller->SetFailed( "Invalid host_data format!");
            return;
        }
        m_ip = host_data.substr(0, m_idx);  // 从查询结果中提取IP地址
        KrpcLogger::Info("ip: "+m_ip );
        m_port = atoi(host_data.substr(m_idx + 1, host_data.size() - m_idx).c_str());  // 从查询结果中提取端口号
        KrpcLogger::Info("port: " + m_port );
        std::cout << "选中节点：" << m_ip << ":" << m_port << std::endl;


        // 尝试连接服务器
        auto rt = newConnect(m_ip.c_str(), m_port);
        if (!rt) {
            std::cout << "connect server error";  // 连接失败，记录错误日志
            controller->SetFailed("connect server error");  // ✅ 加这句
            return;
        } else {
            std::cout << "connect server success";  // 连接成功，记录日志
        }

        //设置连接超时，返回错误
        struct timeval timeout;
        timeout.tv_sec = 4;   //4秒超时示例
        timeout.tv_usec = 0;
        setsockopt(m_clientfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    }  // endif
    std::cout<<"krpcchannel"<< std::endl;
    // 将请求参数序列化为字符串，并计算其长度
    uint32_t args_size{};
    std::string args_str;
    if (request->SerializeToString(&args_str)) {  // 序列化请求参数
        args_size = args_str.size();  // 获取序列化后的长度
    } else {
        controller->SetFailed("serialize request fail");  // 序列化失败，设置错误信息
        return;
    }

    // 定义RPC请求的头部信息
    Krpc::RpcHeader krpcheader;
    krpcheader.set_service_name(service_name);  // 设置服务名
    krpcheader.set_method_name(method_name);  // 设置方法名
    krpcheader.set_args_size(args_size);  // 设置参数长度

    // 将RPC头部信息序列化为字符串，并计算其长度
    uint32_t header_size = 0;
    std::string rpc_header_str;
    if (krpcheader.SerializeToString(&rpc_header_str)) {  // 序列化头部信息
        header_size = rpc_header_str.size();  // 获取序列化后的长度
    } else {
        controller->SetFailed("serialize rpc header error!");  // 序列化失败，设置错误信息
        return;
    }

    // 将头部长度和头部信息拼接成完整的RPC请求报文
    std::string send_rpc_str;
    {
        google::protobuf::io::StringOutputStream string_output(&send_rpc_str);
        google::protobuf::io::CodedOutputStream coded_output(&string_output);
        coded_output.WriteVarint32(static_cast<uint32_t>(header_size));  // 写入头部长度(header_size)
        coded_output.WriteString(rpc_header_str);  // 写入头部信息（service_name+method_name+args_size）
    }
    send_rpc_str += args_str;  // 拼接请求参数(args)

    // 发送RPC请求到服务器
    if (-1 == send(m_clientfd, send_rpc_str.c_str(), send_rpc_str.size(), 0)) {
        close(m_clientfd);  // 发送失败，关闭socket
        char errtxt[512] = {};
        //KrpcLogger::ERROR( "send error: " + tostring(strerror_r(errno, errtxt, sizeof(errtxt))));
        std::cout << "send error: " << strerror_r(errno, errtxt, sizeof(errtxt)) << std::endl;  // 打印错误信息
        controller->SetFailed(errtxt);  // 设置错误信息
        return;
    }

    // 接收服务器的响应
    char recv_buf[1024] = {0};
    int recv_size = recv(m_clientfd, recv_buf, sizeof(recv_buf), 0);
    if (recv_size == -1) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            controller->SetFailed("recv timeout");
        } else {
            char errtxt[512] = {};
            strerror_r(errno, errtxt, sizeof(errtxt));
            controller->SetFailed(errtxt);
        }
        close(m_clientfd);
        m_clientfd = -1;
        return;
    }
    // int recv_size = 0;
    // if (-1 == (recv_size = recv(m_clientfd, recv_buf, 1024, 0))) {
    //     char errtxt[512] = {};
    //     std::cout << "recv error" << strerror_r(errno, errtxt, sizeof(errtxt)) << std::endl;  // 打印错误信息
    //     controller->SetFailed(errtxt);  // 设置错误信息
    //     return;
    // }

    // 将接收到的响应数据反序列化为response对象
    if (!response->ParseFromArray(recv_buf, recv_size)) {
        close(m_clientfd);  // 反序列化失败，关闭socket
        char errtxt[512] = {};
        std::cout << "parse error" << strerror_r(errno, errtxt, sizeof(errtxt)) << std::endl;  // 打印错误信息
        controller->SetFailed(errtxt);  // 设置错误信息
        return;
    }

    close(m_clientfd);  // 关闭socket连接
    m_clientfd=-1;  
}

// void KrpcChannel::CallMethod(
//     const ::google::protobuf::MethodDescriptor *method,
//     ::google::protobuf::RpcController *controller,
//     const ::google::protobuf::Message *request,
//     ::google::protobuf::Message *response,
//     ::google::protobuf::Closure *done)
// {
//     // -------------------------------
//     // 1. 解析服务名、方法名
//     // -------------------------------
//     const auto *sd = method->service();
//     std::string service_name = sd->name();
//     std::string method_name = method->name();

//     ZkClient zkCli;
//     zkCli.Start();

//     // -------------------------------
//     // 2. 拉取所有可用节点并做熔断判断
//     // -------------------------------
//     std::string service_method_path = '/' + service_name + '/' + method_name;
//     std::vector<std::string> children = zkCli.GetChildren(service_method_path);

//     std::vector<std::string> candidates;
//     for (auto &child : children) {
//         std::string method_ipport = service_name + "/" + method_name + "/" + child;
//         CircuitBreakerManager::set_service_method_ipport(method_ipport);
//         if (!CircuitBreakerManager::GetInstance().ShouldSkip(method_ipport)) {
//             candidates.push_back(child);
//         }
//     }

//     if (candidates.empty()) {
//         std::cerr << "[CB] All nodes skipped due to OPEN state." << std::endl;
//         controller->SetFailed("No available provider node due to circuit break.");
//         return;
//     }

//     std::cout<<"krpc callmethod"<<std::endl;
//     // // -------------------------------
//     // // 3. 负载均衡：随机选择一个可用节点
//     // // -------------------------------
//     // int index = rand() % candidates.size();
//     // std::string host_data = candidates[index]; // "127.0.0.1:8000"
//     // size_t idx = host_data.find(':');
//     // if (idx == std::string::npos) {
//     //     controller->SetFailed("Invalid host_data format!");
//     //     return;
//     // }

//     // -------------------------------
//     // 轮询负载均衡
//     // -------------------------------
//     uint64_t index = round_counter_.fetch_add(1);
//     index = index % candidates.size();
//     std::string host_data = candidates[index];

//     size_t idx = host_data.find(':');
//     if (idx == std::string::npos) {
//         controller->SetFailed("Invalid host_data format!");
//         return;
//     }

//     std::string ip = host_data.substr(0, idx);
//     int port = atoi(host_data.substr(idx + 1).c_str());

//     // -------------------------------
//     // 4. 建立短连接
//     // -------------------------------
//     int clientfd = socket(AF_INET, SOCK_STREAM, 0);
//     if (clientfd == -1) {
//         controller->SetFailed("create socket error");
//         return;
//     }

//     sockaddr_in server_addr{};
//     server_addr.sin_family = AF_INET;
//     server_addr.sin_port = htons(port);
//     inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr);

//     if (connect(clientfd, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
//         close(clientfd);
//         controller->SetFailed("connect error");
//         return;
//     }

//     // -------------------------------
//     // 5. 序列化 RPC 请求
//     // -------------------------------
//     std::string args_str;
//     if (!request->SerializeToString(&args_str)) {
//         close(clientfd);
//         controller->SetFailed("serialize request fail");
//         return;
//     }

//     Krpc::RpcHeader rpc_header;
//     rpc_header.set_service_name(service_name);
//     rpc_header.set_method_name(method_name);
//     rpc_header.set_args_size(args_str.size());

//     std::string rpc_header_str;
//     if (!rpc_header.SerializeToString(&rpc_header_str)) {
//         close(clientfd);
//         controller->SetFailed("serialize rpc header fail");
//         return;
//     }

//     std::string send_rpc_str;
//     google::protobuf::io::StringOutputStream string_output(&send_rpc_str);
//     google::protobuf::io::CodedOutputStream coded_output(&string_output);
//     coded_output.WriteVarint32(rpc_header_str.size());
//     coded_output.WriteString(rpc_header_str);
//     send_rpc_str += args_str;

//     if (send(clientfd, send_rpc_str.c_str(), send_rpc_str.size(), 0) == -1) {
//         close(clientfd);
//         controller->SetFailed("send error");
//         return;
//     }

//     // -------------------------------
//     // 6. 接收响应
//     // -------------------------------
//     char recv_buf[4096] = {0};
//     int recv_size = recv(clientfd, recv_buf, sizeof(recv_buf), 0);
//     if (recv_size <= 0) {
//         close(clientfd);
//         controller->SetFailed("recv error");
//         return;
//     }

//     if (!response->ParseFromArray(recv_buf, recv_size)) {
//         close(clientfd);
//         controller->SetFailed("parse response fail");
//         return;
//     }

//     // -------------------------------
//     // 7. 正常关闭短连接
//     // -------------------------------
//     close(clientfd);

//     // -------------------------------
//     // 8. 回调 done（可选）
//     // -------------------------------
//     if (done) done->Run();
// }


// 创建新的socket连接
bool KrpcChannel::newConnect(const char *ip, uint16_t port) {
    // 创建socket
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == clientfd) {
        char errtxt[512] = {0};
        std::cout << "socket error" << strerror_r(errno, errtxt, sizeof(errtxt)) << std::endl;  // 打印错误信息
        return false;
    }

    // 设置服务器地址信息
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;  // IPv4地址族
    server_addr.sin_port = htons(port);  // 端口号
    server_addr.sin_addr.s_addr = inet_addr(ip);  // IP地址

    // 尝试连接服务器
    if (-1 == connect(clientfd, (struct sockaddr *)&server_addr, sizeof(server_addr))) {
        close(clientfd);  // 连接失败，关闭socket
        char errtxt[512] = {0};
        std::cout << "connect error" << strerror_r(errno, errtxt, sizeof(errtxt)) << std::endl;  // 打印错误信息
        return false;
    }

    m_clientfd = clientfd;  // 保存socket文件描述符
    return true;
}

// 从ZooKeeper查询服务地址
std::string KrpcChannel::QueryServiceHost(ZkClient *zkclient, std::string service_name, std::string method_name, int &idx) {
    std::string method_path = "/" + service_name + "/" + method_name;  // 构造ZooKeeper路径
    std::cout << "method_path: " << method_path << std::endl;

    std::unique_lock<std::mutex> lock(g_data_mutx);  // 加锁，保证线程安全，保证并发场景下只有一个线程可以访问到zookeeper服务器
    std::string host_data_1 = zkclient->GetData(method_path.c_str());  // 从ZooKeeper获取数据
    lock.unlock();  // 解锁

    if (host_data_1 == "") {  // 如果未找到服务地址
       std::cout << method_path + " is not exist!";  // 记录错误日志
        return " ";
    }

    idx = host_data_1.find(":");  // 查找IP和端口的分隔符
    if (idx == -1) {  // 如果分隔符不存在
        std::cout  << method_path + " address is invalid!";  // 记录错误日志
        return " ";
    }

    return host_data_1;  // 返回服务地址
}

// 构造函数，支持延迟连接
KrpcChannel::KrpcChannel(bool connectNow) : m_clientfd(-1), m_idx(0) {
    if (!connectNow) {  // 如果不需要立即连接
        return;
    }

    
    // 尝试连接服务器，最多重试3次
    auto rt = newConnect(m_ip.c_str(), m_port);
    int count = 3;  // 重试次数
    while (!rt && count--) {
        rt = newConnect(m_ip.c_str(), m_port);
    }
}