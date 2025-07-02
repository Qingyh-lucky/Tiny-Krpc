#ifndef KRPC_LOG_H
#define KRPC_LOG_H
#include<glog/logging.h>
#include<string>
// 采用RAII的思想
class KrpcLogger
{
public:
      //构造函数，自动初始化glog
      explicit KrpcLogger(const char *argv0,const std::string& log_dir ="")
      {
        google::InitGoogleLogging(argv0);
        FLAGS_colorlogtostderr=false;//关闭彩色日志
        FLAGS_logtostderr=false;//不输出到stderr
        if (!log_dir.empty()) {
            FLAGS_log_dir = log_dir;  // 设置日志文件存储目录
        }
      }
      ~KrpcLogger(){
        google::FlushLogFiles(google::GLOG_INFO);
        google::FlushLogFiles(google::GLOG_WARNING);
        google::FlushLogFiles(google::GLOG_ERROR);
        google::FlushLogFiles(google::GLOG_FATAL);
        google::ShutdownGoogleLogging();
      }
      //提供静态日志方法
      static void Info(const std::string &message)
      {
        LOG(INFO)<<message;
      }
      static void Warning(const std::string &message){
        LOG(WARNING)<<message;
      }
      static void ERROR(const std::string &message){
        LOG(ERROR)<<message;
      }
      static void Fatal(const std::string& message) {
        LOG(FATAL) << message;
    }
//禁用拷贝构造函数和重载赋值函数
private:
    KrpcLogger(const KrpcLogger&)=delete;
    KrpcLogger& operator=(const KrpcLogger&)=delete;
};



#endif