/*-
 * Copyright (c) 2018 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */

#include <xtra_rhel6.h>

#include <cmath>
#include <set>
 
 // 无状态的数据聚合计算

#include <Utils/Log.h>

#include <Business/EventTypes.h>
#include <Business/EventItem.h>
#include <Business/EventRepos.h>

using namespace tzrpc;


extern int stateless_process_event(events_ptr_t event, event_insert_t copy_stat);

// 内部使用临时结构体
struct stat_info_t {
    int count;
    int64_t value_sum;
    int64_t value_avg;
    double  value_std;
    std::vector<int64_t> values;
};

static 
void calc_event_stage1(const event_data_t& data, std::map<std::string, stat_info_t>& infos) {
    if (infos.find(data.tag) == infos.end()) {
        infos[data.tag] = stat_info_t {};
    }
    infos[data.tag].values.push_back(data.value);
    infos[data.tag].count += 1;
    infos[data.tag].value_sum += data.value;
}

static 
void calc_event_info(std::vector<event_data_t>& data,
                     std::map<std::string, stat_info_t>& infos) {

    // 消息检查和排重
    std::set<int64_t> ids;
    for (auto iter = data.begin(); iter != data.end(); ++iter) {
        ids.insert(iter->msgid);
    }
    if (ids.size() != data.size()) {
        log_err("mismatch size, may contain duplicate items: %d - %d",
                static_cast<int>(ids.size()), static_cast<int>(data.size()));

#if 0
        for(size_t i=0; i<data.size(); ++i) {
            for(size_t j=i+1; j<data.size(); ++j) {
                if (data[i].msgid == data[j].msgid) {
                    log_err("found dumpicate message: %lu - %lu, (%ld %ld %s), (%ld %ld %s"), i, j,
                            data[i].msgid, data[i].value, data[i].flag.c_str(),
                            data[j].msgid, data[j].value, data[j].flag.c_str());
                }
            }
        }
#endif

        size_t expected_size = ids.size();
        // 进行去重复
        std::vector<event_data_t> new_data;
        for (auto iter = data.begin(); iter != data.end(); ++iter) {
            if (ids.find(iter->msgid) != ids.end()) {
                new_data.push_back(*iter);
                ids.erase(iter->msgid);
            }
        }

        SAFE_ASSERT(new_data.size() == expected_size);
        data = std::move(new_data);
    }


    std::for_each(data.begin(), data.end(),
                  std::bind(calc_event_stage1, std::placeholders::_1, std::ref(infos)));

    // calc avg and std
    for (auto iter = infos.begin(); iter!= infos.end(); ++iter) {
        auto& info = iter->second;
        info.value_avg = info.value_sum / info.count;

        double sum = 0;
        for (size_t i=0; i< info.values.size(); ++i) {
            sum += ::pow(info.values[i] - info.value_avg, 2);
        }
        info.value_std = ::sqrt(sum / info.values.size());
    }
}

int stateless_process_event(events_ptr_t event, event_insert_t copy_stat) {

    auto& event_slot = event->data_;

    // process event
    for (auto iter = event_slot.begin(); iter != event_slot.end(); ++iter) {
        auto& events_metric = iter->first;
        auto& events_info = iter->second;
        log_debug("process event %s, count %d", events_metric.c_str(), static_cast<int>(events_info.size()));

        std::map<std::string, stat_info_t> tag_info;
        calc_event_info(events_info, tag_info);

        // log_debug("process reuslt for event: %s", events_name.c_str());
        for (auto it = tag_info.begin(); it != tag_info.end(); ++it) {
            log_debug("tag %s, count %d, value %ld",
                      it->first.c_str(),
                      static_cast<int>(tag_info[it->first].count), tag_info[it->first].value_sum);

            SAFE_ASSERT(tag_info[it->first].count != 0);

            copy_stat.metric = events_metric;
            copy_stat.tag = it->first;
            copy_stat.count = tag_info[it->first].count;
            copy_stat.value_sum = tag_info[it->first].value_sum;
            copy_stat.value_avg = tag_info[it->first].value_avg;
            copy_stat.value_std = tag_info[it->first].value_std;

            if (EventRepos::instance().store()->insert_ev_stat(copy_stat) != 0) {
                log_err("store for (%s, %s) - %ld name:%s, flag:%s failed!",
                        copy_stat.service.c_str(), copy_stat.entity_idx.c_str(),
                        copy_stat.timestamp, copy_stat.metric.c_str(), copy_stat.tag.c_str());
            } else {
                log_debug("store for (%s, %s) - %ld name:%s, flag:%s ok!",
                          copy_stat.service.c_str(), copy_stat.entity_idx.c_str(),
                          copy_stat.timestamp, copy_stat.metric.c_str(), copy_stat.tag.c_str());
            }
        }
    }

    return 0;
}
