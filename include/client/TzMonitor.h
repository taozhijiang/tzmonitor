#ifndef _TZ_MONITOR_CLIENT_H__
#define _TZ_MONITOR_CLIENT_H__

// 此库的作用就是对业务服务产生的数据进行压缩，然后开辟
// 独立的线程向服务端进行提交，以避免多次独立提交带来的额外消耗

// 客户端使用，尽量减少依赖的库

#include <memory>
#include <thread>
#include <functional>
#include <boost/noncopyable.hpp>

template<class T>
T * get_pointer(std::shared_ptr<T> const& p) {
    return p.get();
}

#define LOG printf

namespace TzMonitor {

typedef std::shared_ptr<std::thread> thread_ptr;

class TzMonitorClient: public boost::noncopyable,
                       public std::enable_shared_from_this<TzMonitorClient> {
public:
    TzMonitorClient(std::string host, std::string serv, std::string entity_idx = "1") :
        host_(host), serv_(serv), entity_idx_(entity_idx) {
        }

    bool init() {
        thread_ptr t_thread(new std::thread(std::bind(&TzMonitorClient::run, shared_from_this())));
        if (!t_thread){
            LOG("create work thread failed! ");
            return false;
        }

        threads_.push_back(t_thread);
        return true;
    }

    void run() {

    }

    int report_event(const std::string& name, int64_t value) {
        return 0;
    }

    ~TzMonitorClient() {}

private:
    std::string host_;
    std::string serv_;
    std::string entity_idx_;

    // 默认开启一个，当发现待提交队列过长的时候，自动开辟独立的工作线程
    std::vector<thread_ptr> threads_;
};

} // end namespace

#endif // _TZ_MONITOR_CLIENT_H__
