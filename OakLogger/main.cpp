#include<iostream>
#include<string>
#include<vector>
#include<sstream>
#include<fstream>
#include<mutex>
#include <queue>
#include <thread>
#include <condition_variable>
#include<atomic>

//辅助函数，利用原样转发
template<typename T>
std::string Oak_ToString(T&& args){
    std::ostringstream oss;
    oss << std::forward<T>(args);
    return oss.str();
}

class LogQueue{
public:
    void push(const std::string& msg){
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push(msg);
        m_cond.notify_one();
    }
    bool pop(std::string& msg){
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cond.wait(lock,[this]{return !m_queue.empty()|| m_isShutdown;});//防止操作系统虚假唤醒 

        if(m_isShutdown|| m_queue.empty()) return false;

        msg=m_queue.front();
        m_queue.pop();
        return true;
    }
    void shutdown(){
        std::lock_guard<std::mutex> lock(m_mutex);
        m_isShutdown= true;
        m_cond.notify_all();
    };

private:
    std::queue<std::string> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_cond;
    bool m_isShutdown=false;
};

class Logger{
public:
    Logger(const std::string& filename):m_logfile(filename,std::ios::app|std::ios::out),
    m_exitFlag(false){
        if(!m_logfile.is_open()){
            throw std::runtime_error("Failed to open log file\n");
        }
        m_thread=std::thread([this](){
            std::string msg;
            while(m_queue.pop(msg)){
                m_logfile<<msg<<std::endl;
            }
        });
    }
    ~Logger(){
        m_queue.shutdown();

        if(m_thread.joinable()) m_thread.join();

        if(m_logfile.is_open()) m_logfile.close();
    }

    template<typename... Args>
    void log(const std::string& format,Args&&... args){
        m_queue.push(formatMessage(format,std::forward<Args>(args)...));
    }

    

private:
    LogQueue m_queue;
    std::thread m_thread;
    std::ofstream m_logfile;
    std::atomic<bool> m_exitFlag;

    template<typename... Args>
    std::string formatMessage(const std::string& format,Args&&... args){
       std::vector<std::string> args_strings= {Oak_ToString(std::forward<Args>(args))...};
       std::ostringstream oss;
       size_t args_index=0;
       size_t pos=0;
       size_t placeholder=0;
       placeholder=format.find("{}",pos);
       while(placeholder!=std::string::npos){
            oss<<format.substr(pos,placeholder-pos);
            if(args_index<args_strings.size())
                oss<<args_strings[args_index++];
            else oss<<"{}";
            pos=placeholder+2;
            placeholder = format.find("{}", pos);
       }
       oss<<format.substr(pos);
       while(args_index<args_strings.size()) oss<<args_strings[args_index++];

       return oss.str();
    }
};

int main(){
    try {
        Logger logger("log.txt");

        logger.log("Starting application.");

        int user_id = 42;
        std::string action = "login";
        double duration = 3.5;
        std::string world = "World";

        logger.log("User {} performed {} in {} seconds.", user_id, action, duration);
        logger.log("Hello {}", world);
        logger.log("This is a message without placeholders.");
        logger.log("Multiple placeholders: {}, {}, {}.", 1, 2, 3);

        // 模拟一些延迟以确保后台线程处理完日志
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }
    catch (const std::exception& ex) {
        std::cerr << "日志系统初始化失败: " << ex.what() << std::endl;
    }

    return 0;
}