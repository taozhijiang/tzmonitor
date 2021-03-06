/*-
 * Copyright (c) 2019 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */


#ifndef __UTILS_TINY_TASK_H__
#define __UTILS_TINY_TASK_H__


#include <xtra_rhel.h>

// 本来想直接使用future来做的，但是现有生产环境太旧，std::future
// 和boost::future都不可用，这里模拟一个任务结构，采用one-task-per-thread的方式，
// 进行弹性的任务创建执行。

#include <memory>
#include <functional>

#include <Utils/ThreadMng.h>
#include <Utils/EQueue.h>

namespace tzrpc {

class TinyTask: public std::enable_shared_from_this<TinyTask> {

public:

    explicit TinyTask(uint8_t max_spawn_task):
        max_spawn_task_(max_spawn_task),
        thread_run_(),
        thread_terminate_(false),
        thread_mng_(max_spawn_task),
        tasks_() {
    }

    ~TinyTask() {
        thread_terminate_ = true;
        if(thread_run_ && thread_run_->joinable()) {
            thread_run_->join();
        }
    }

    // 禁止拷贝
    TinyTask(const TinyTask&) = delete;
    TinyTask& operator=(const TinyTask&) = delete;

    bool init(){
        thread_run_.reset(new boost::thread(std::bind(&TinyTask::run, shared_from_this())));
        if (!thread_run_){
            printf("create run work thread failed! ");
            return false;
        }

        return true;
    }

    void add_additional_task(const TaskRunnable& func) {
        tasks_.PUSH(func);
    }

    // 返回大致可用的空闲线程数目
    uint32_t available() {
        return max_spawn_task_ - thread_mng_.current_size();
    }

    void modify_spawn_size(uint32_t nsize) {
        
        if (nsize == 0) {
            printf("invalid new max_spawn_task size: %u", nsize);
            return;
        }

        if (nsize != max_spawn_task_) {
            printf("update ThreadMng task size from %u to %u",
                   max_spawn_task_, nsize);
            max_spawn_task_ = nsize;

            thread_mng_.modify_spawn_size(nsize);
        }
    }


private:
    void run() {

        printf("TinyTask thread %#lx begin to run ...", (long)pthread_self());

        while (true) {
            
            if( thread_terminate_ ) {
                log_debug("TinyTask thread %#lx about to terminate ...", (long)pthread_self());
                break;
            }

            std::vector<TaskRunnable> tasks {};
            size_t count = tasks_.POP(tasks, max_spawn_task_, 1000);
            if( !count ){  // 空闲
                continue;
            }

            for(size_t i=0; i<tasks.size(); ++i) {
                thread_mng_.add_task(tasks[i]);
            }

            thread_mng_.join_all();

            printf("count %d task process done!", static_cast<int>(tasks.size()));
        }
    }

private:

    uint32_t max_spawn_task_;
    std::shared_ptr<boost::thread> thread_run_;
    bool thread_terminate_;

    // 封装线程管理细节
    ThreadMng thread_mng_;

    // 待单独执行的任务列表
    EQueue<TaskRunnable> tasks_;
};


} // end namespace tzrpc

#endif // __UTILS_TINY_TASK_H__
