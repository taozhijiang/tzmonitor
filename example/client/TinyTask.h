#ifndef __TZ_TINY_TASK_H__
#define __TZ_TINY_TASK_H__


// 本来想直接使用future来做的，但是现有生产环境太旧，std::future
// 和boost::future都不可用，这里模拟一个任务结构，采用one-task-per-thread的方式，
// 进行弹性的任务创建执行。

#include <memory>
#include <thread>
#include <functional>

#include <boost/noncopyable.hpp>

#define LOG printf

typedef std::function<void()> TaskRunnable;

class TinyTask: private boost::noncopyable,
                public std::enable_shared_from_this<TinyTask> {
public:

    explicit TinyTask(uint8_t max_spawn_task):
       max_spawn_task_(max_spawn_task){
       }
       
    ~TinyTask() {}

    bool init(){
        thread_run_.reset(new std::thread(std::bind(&TinyTask::run, shared_from_this())));
        if (!thread_run_){
            LOG("create run work thread failed! ");
            return false;
        }
        
        return true;
    }
    
    void add_task(const TaskRunnable& func) {
        tasks_.PUSH(func);
    }
    
private:
    void run() {
        
        LOG("TinyTask thread %#lx begin to run ...", (long)pthread_self());
        while (true) {

            std::vector<TaskRunnable> task;
            size_t count = tasks_.POP(task, max_spawn_task_, 5000);
            if( !count ){
                continue;
            }
            
            std::vector<std::thread> thread_group;
            for(size_t i=0; i<task.size(); ++i) {
                thread_group.emplace_back(std::thread(task[i]));
            }
            
            for(size_t i=0; i<task.size(); ++i) {
                if(thread_group[i].joinable())
                    thread_group[i].join();
            }
            
            LOG("count %d task process done!", static_cast<int>(task.size()));
        }
    }

private:
    const uint32_t max_spawn_task_;

    std::shared_ptr<std::thread> thread_run_;
    EQueue<TaskRunnable> tasks_;
};


#endif // __TZ_TINY_TASK_H__
