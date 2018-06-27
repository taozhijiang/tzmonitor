#include <iostream>

#include <event2/event.h>
#include <event2/util.h>

#include "General.h"
#include "Helper.h"
#include <utils/Log.h>

#include "TSv2.h"

bool TimerService::init() {

    struct event_config *cfg;
    /*带配置产生event_base对象*/
    cfg = event_config_new();
    event_config_avoid_method(cfg, "select");   //避免使用select
    event_config_require_features(cfg, EV_FEATURE_ET);  //使用边沿触发类型
    //event_config_set_flag(cfg, EVENT_BASE_FLAG_PRECISE_TIMER);
    //event_base_new(void); 为根据系统选择最快最合适的类型
    ev_base_ = event_base_new_with_config(cfg);
    event_config_free(cfg);

    if (!ev_base_) {
        log_err("Creating event_base failed!");
        return false;
    }

    log_info("Current Using Method: %s", event_base_get_method(ev_base_)); // epoll

    // priority 0, and priority 1 ->　lower
    // 0: listen  1: read/write 2: other
    event_base_priority_init(ev_base_, 3);

    // add purge task
    if (register_timer_task(std::bind(&TimerService::purge_dead_task, this), 5*1000, true, true) == 0) {
        log_err("Register purge task failed!");
        return false;
    }

    return true;
}

// 因为libevent的回调都是C的，而C++中很多回调函数将是C++ margin的，必须要有个wrapper
void c_timer_cb(int fd, short what, void *arg) {

    auto self = helper::request_timer_service();
    self->timer_cb(fd, what, arg);
}

void TimerService::timer_cb (int fd, short what, void *arg) {

    if (fd != -1 || !(what & (EV_READ | EV_TIMEOUT) ) ) {
        log_err("Invalid check: %d, %d, %d, %d ", fd, what, EV_READ, EV_TIMEOUT);
        return;
    }

    TimerTaskPtr task_ptr = find_task(reinterpret_cast<int64_t>(arg));
    if (!task_ptr) {
        log_err("Task %ld not found, just return.", reinterpret_cast<int64_t>(arg));
        return;
    }

    // 肯定是串行的，主线程只有一个，所以不会有竞争条件
    if (task_ptr->fast_) {
        task_ptr->func_();
        if (!task_ptr->persist_) {
            event_free(task_ptr->ev_timer_);
            task_ptr->dead_ = true;
        }
    } else {
        defer_ready_.PUSH(task_ptr->func_); // func_是拷贝的，无所谓
        if (!task_ptr->persist_) {
            event_free(task_ptr->ev_timer_);
            task_ptr->dead_ = true;
        }
    }
}


void TimerService::timer_run() {

    //event_base_dispatch(base); //进入事件循环直到没有pending的事件就返回
    //EVLOOP_ONCE　阻塞直到有event激活，执行回调函数后返回
    //EVLOOP_NONBLOCK 非阻塞类型，立即检查event激活，如果有运行最高优先级的那一类，完毕后退出循环

    event_base_loop(ev_base_, 0);
    log_err("event_base_loop terminating here... ");
}

void TimerService::timer_defer_run(ThreadObjPtr ptr){

    log_info("TimerService thread %#lx is about to work... ", (long)pthread_self());

    TimerEventCallable func;

    while (true) {

        if (unlikely(ptr->status_ == ThreadStatus::kThreadTerminating)) {
            log_err("thread %#lx is about to terminating...", (long)pthread_self());
            break;
        }

        // 线程启动
        if (unlikely(ptr->status_ == ThreadStatus::kThreadSuspend)) {
            ::usleep(500*1000);
            continue;
        }

        if (!defer_ready_.POP(func, 3 * 1000 /*3s*/)) {
            // yk_api::log_info("timer task timeout return!");
            continue;
        }

        if (!func){
            log_err("timer task empty func return!");
            continue;
        }

        func(); // call it
    }

    ptr->status_ = ThreadStatus::kThreadDead;
    log_info("TimerService thread %#lx is about to terminate ... ", (long)pthread_self());

    return;
}

int TimerService::start_timer(){

    timer_thread_ = std::thread(std::bind(&TimerService::timer_run, this));

    if (! timer_defer_.init_threads(std::bind(&TimerService::timer_defer_run, this, std::placeholders::_1))) {
        log_err("TimerService::init failed!");
        return -1;
    }
    timer_defer_.start_threads();

    return 0;
}

int TimerService::stop_graceful() {
    {
        std::map<int64_t, TimerTaskPtr>::iterator it, tmp;
        std::lock_guard<std::mutex> lock(tasks_lock_);

        for (it = tasks_.begin(); it != tasks_.end(); /**/ ) {
            event_free(it->second->ev_timer_);
            log_info("purge task %ld", it->first);
            tmp = it ++;
            tasks_.erase(tmp);
        }
    }

    log_info("CURRENT PENDING WORK: %lu", defer_ready_.SIZE());
    timer_defer_.graceful_stop_threads();

    return 0;
}

int TimerService::join() {
    timer_defer_.join_threads();
    return 0;
}

int64_t TimerService::register_timer_task(TimerEventCallable func, int64_t msec,
                                          bool persist, bool fast) {

    struct event *ev_timer;
    struct timeval tv = { 0, 0 };
    tv.tv_usec = (msec % 1000) * 1000;
    tv.tv_sec = msec / 1000;

    TimerTaskPtr task_ptr = std::make_shared<TimerTask>(ev_timer, func, tv, persist, fast);
    if (!task_ptr) {
        log_err("create TimerTask failed!");
        return 0;
    }
    int64_t task_idx = reinterpret_cast<int64_t>(task_ptr.get());

    if(persist) {
        ev_timer = event_new(ev_base_, -1, EV_PERSIST, c_timer_cb, (void*)task_idx);
    } else {
        ev_timer = event_new(ev_base_, -1, 0, c_timer_cb, (void*)task_idx);
    }

    if (!ev_timer) {
        log_err("create ev_timer failed!");
        return 0;
    }

    event_priority_set(ev_timer, 2);
    event_add(ev_timer, &tv);
    add_task(task_ptr);

    return task_idx;
}

bool TimerService::revoke_timer_task(int64_t index) {

    TimerTaskPtr ret;

    std::lock_guard<std::mutex> lock(tasks_lock_);
    std::map<int64_t, TimerTaskPtr>::iterator it = tasks_.find(index);
    if (it != tasks_.end()) {
        event_free(it->second->ev_timer_);
    }
    return !!tasks_.erase(index);
}

TimerTaskPtr TimerService::find_task(int64_t index) {

    TimerTaskPtr ret;

    std::lock_guard<std::mutex> lock(tasks_lock_);
    std::map<int64_t, TimerTaskPtr>::iterator it = tasks_.find(index);
    if (it != tasks_.end()) {
        ret = it->second;
    }

    return ret;
}

int TimerService::add_task(TimerTaskPtr task_ptr) {

    if (!task_ptr)
        return -1;

    int64_t task_idx = reinterpret_cast<int64_t>(task_ptr.get());
    std::lock_guard<std::mutex> lock(tasks_lock_);
    std::map<int64_t, TimerTaskPtr>::iterator it = tasks_.find(task_idx);
    if (it != tasks_.end()) {
        log_err("The timer task already found: %ld", task_idx);
        return -1;
    }

    tasks_[task_idx] = task_ptr; // add here

    return 0;
}

// 删除dead_的定时任务，这个函数只可能在主线程中串行执行，
// bf线程的执行函数都是拷贝构造的，所以不会有安全问题
void TimerService::purge_dead_task(){

    log_debug("purge_dead_task check ...");

    std::lock_guard<std::mutex> lock(tasks_lock_);
    std::map<int64_t, TimerTaskPtr>::iterator it;
    std::map<int64_t, TimerTaskPtr>::iterator tmp;

    for (it = tasks_.begin(); it != tasks_.end(); /**/ ) {
        if(it->second->dead_) {
            log_info("purge task %ld", it->first);
            tmp = it ++;
            tasks_.erase(tmp);
        } else {
            ++it;
        }
    }
}

