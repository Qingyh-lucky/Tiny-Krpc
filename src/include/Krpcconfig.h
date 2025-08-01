#ifndef _Krpcconfig_h
#define _Krpcconfig_h
#include <unordered_map>
#include <string>

//框架读取配置文件类
class Krpcconfig{
    public:
    void LoadConfigFile(const char *config_file);//加载配置文件
    std::string Load(const std::string &key);//查找key对应的value
    private:
    std::unordered_map<std::string, std::string> config_map;
    void Trim(std::string &read_buf);//去掉字符串前后的空格
};
#endif
