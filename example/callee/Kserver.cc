#include <iostream>
#include <string>
#include <thread>
#include <queue>
#include <condition_variable>
#include "../user.pb.h"
#include "Krpcapplication.h"
#include "Krpcprovider.h"
/*
UserService 原本是一个本地服务，提供了两个本地方法：Login 和 GetFriendLists。
现在通过 RPC 框架，这些方法可以被远程调用。
*/
class UserService : public Kuser::UserServiceRpc // 继承自 protobuf 生成的 RPC 服务基类
{
private:
    bool simulate_error_ = false;    //模拟错误
    bool simulate_timeout_ = false;  //模拟超时
    DistributedCounter counter_;  // ✅ 成员变量
public:

    UserService(): counter_("127.0.0.1", 6379) {  // 连接到本地 Redis 服务器{
         // 从配置文件加载参数
        simulate_error_=stoi(KrpcApplication::GetInstance().GetConfig().Load("simulate_error"));
        simulate_timeout_=stoi(KrpcApplication::GetInstance().GetConfig().Load("simulate_timeout"));
        std::cout<<"simulate_error_"<<simulate_error_<<"simulate_timeout_"<<simulate_timeout_<<std::endl;
    }
    
    // 本地登录方法，用于处理实际的业务逻辑
    bool Login(std::string name, std::string pwd) {
        std::cout << "doing local service: Login" << std::endl;
        std::cout << "name:" << name << " pwd:" << pwd << std::endl;  
        // usleep(100);
        return true;  // 模拟登录成功
    }

    /*
    重写基类 UserServiceRpc 的虚函数，这些方法会被 RPC 框架直接调用。
    1. 调用者（caller）通过 RPC 框架发送 Login 请求。
    2. 服务提供者（callee）接收到请求后，调用下面重写的 Login 方法。
    */
    void Login(::google::protobuf::RpcController* controller,
              const ::Kuser::LoginRequest* request,
              ::Kuser::LoginResponse* response,
              ::google::protobuf::Closure* done) {
        // 从请求中获取用户名和密码
        std::string name = request->name();
        std::string pwd = request->pwd();
        
       


        // 调用本地业务逻辑处理登录
        bool login_result = Login(name, pwd); 

        //   // 增加调用次数计数器
        // counter_.Increment("login_request_count");

        // // 获取当前计数器值并打印
        // int current_count = counter_.GetValue("login_request_count");
        // std::cout << "Current login request count: " << current_count << std::endl;
        // if (simulate_error_ &&current_count<5) {
         if (simulate_error_ ) {
            //if (rand() % 3 == 0) {//这个用于判断是否连续
                Kuser::ResultCode *code = response->mutable_result();
                code->set_errcode(1);
                code->set_errmsg("Simulated business error");
                response->set_success(false);
                done->Run();
                return;
        }
        else
        {

        }

        if (simulate_timeout_ ) {
            // if (rand() % 3 == 0) {
                std::this_thread::sleep_for(std::chrono::seconds(5));
           // }
        }

        // 将响应结果写入 response 对象
        Kuser::ResultCode *code = response->mutable_result();
        code->set_errcode(0);  // 设置错误码为 0，表示成功
        code->set_errmsg("");  // 设置错误信息为空
        response->set_success(login_result);  // 设置登录结果

        // 执行回调操作，框架会自动将响应序列化并发送给调用者
        done->Run();
    }
};


int main(int argc, char **argv) {
   
    
    // 调用框架的初始化操作，解析命令行参数并加载配置文件
    KrpcApplication::Init(argc, argv);

    // std::string logpath=KrpcApplication::GetInstance().GetConfig().Load("logdir");
    // KrpcLogger logger(argv[0], logpath.c_str());

  
    // 创建一个 RPC 服务提供者对象
    KrpcProvider provider;

    // 将 UserService 对象发布到 RPC 节点上，使其可以被远程调用
    provider.NotifyService(new UserService());

    // 启动 RPC 服务节点，进入阻塞状态，等待远程的 RPC 调用请求
    provider.Run();



    return 0;
}