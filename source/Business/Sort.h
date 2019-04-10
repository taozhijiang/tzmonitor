/*-
 * Copyright (c) 2019 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */


#ifndef __BUSINESS_SORT_H__
#define __BUSINESS_SORT_H__

#include <iostream>
#include <Business/EventTypes.h>


class Sort {

public:

    static void do_sort(std::vector<event_info_t>& info, const enum OrderByType& btp, const enum OrderType& tp) {

        // 进行结果排序
        switch (btp) {
            case OrderByType::kOrderByTimestamp:
                if (tp == OrderType::kOrderDesc)
                    std::sort(info.begin(), info.end(), Sort::sort_by_timestamp_desc);
                else
                    std::sort(info.begin(), info.end(), Sort::sort_by_timestamp);
                break;

            case OrderByType::kOrderByTag:
                if (tp == OrderType::kOrderDesc)
                    std::sort(info.begin(), info.end(), Sort::sort_by_tag_desc);
                else
                    std::sort(info.begin(), info.end(), Sort::sort_by_tag);
                break;

            case OrderByType::kOrderByCount:
                if (tp == OrderType::kOrderDesc)
                    std::sort(info.begin(), info.end(), Sort::sort_by_count_desc);
                else
                    std::sort(info.begin(), info.end(), Sort::sort_by_count);
                break;


            case OrderByType::kOrderBySum:
                if (tp == OrderType::kOrderDesc)
                    std::sort(info.begin(), info.end(), Sort::sort_by_sum_desc);
                else
                    std::sort(info.begin(), info.end(), Sort::sort_by_sum);
                break;


            case OrderByType::kOrderByAvg:
                if (tp == OrderType::kOrderDesc)
                    std::sort(info.begin(), info.end(), Sort::sort_by_avg_desc);
                else
                    std::sort(info.begin(), info.end(), Sort::sort_by_avg);
                break;


            case OrderByType::kOrderByMin:
                if (tp == OrderType::kOrderDesc)
                    std::sort(info.begin(), info.end(), Sort::sort_by_min_desc);
                else
                    std::sort(info.begin(), info.end(), Sort::sort_by_min);
                break;

            case OrderByType::kOrderByMax:
                if (tp == OrderType::kOrderDesc)
                    std::sort(info.begin(), info.end(), Sort::sort_by_max_desc);
                else
                    std::sort(info.begin(), info.end(), Sort::sort_by_max);
                break;


            case OrderByType::kOrderByP10:
                if (tp == OrderType::kOrderDesc)
                    std::sort(info.begin(), info.end(), Sort::sort_by_p10_desc);
                else
                    std::sort(info.begin(), info.end(), Sort::sort_by_p10);
                break;

            case OrderByType::kOrderByP50:
                if (tp == OrderType::kOrderDesc)
                    std::sort(info.begin(), info.end(), Sort::sort_by_p50_desc);
                else
                    std::sort(info.begin(), info.end(), Sort::sort_by_p50);
                break;

            case OrderByType::kOrderByP90:
                if (tp == OrderType::kOrderDesc)
                    std::sort(info.begin(), info.end(), Sort::sort_by_p90_desc);
                else
                    std::sort(info.begin(), info.end(), Sort::sort_by_p90);
                break;

            default:
                std::cerr << "unknown orderby: "  << static_cast<int32_t>(btp) << std::endl;
                break;
        }

        return;
    }

private:
    // asc

    static bool sort_by_timestamp(const event_info_t& lhs, const event_info_t& rhs)  {
        return lhs.timestamp < rhs.timestamp;
    }

    static bool sort_by_tag(const event_info_t& lhs, const event_info_t& rhs)  {
        return lhs.tag < rhs.tag;
    }

    static bool sort_by_count(const event_info_t& lhs, const event_info_t& rhs)  {
        return lhs.count < rhs.count;
    }

    static bool sort_by_sum(const event_info_t& lhs, const event_info_t& rhs)  {
        return lhs.value_sum < rhs.value_sum;
    }

    static bool sort_by_avg(const event_info_t& lhs, const event_info_t& rhs)  {
        return lhs.value_avg < rhs.value_avg;
    }

    static bool sort_by_min(const event_info_t& lhs, const event_info_t& rhs)  {
        return lhs.value_min < rhs.value_min;
    }

    static bool sort_by_max(const event_info_t& lhs, const event_info_t& rhs)  {
        return lhs.value_max < rhs.value_max;
    }

    static bool sort_by_p10(const event_info_t& lhs, const event_info_t& rhs)  {
        return lhs.value_p10 < rhs.value_p10;
    }

    static bool sort_by_p50(const event_info_t& lhs, const event_info_t& rhs)  {
        return lhs.value_p50 < rhs.value_p50;
    }

    static bool sort_by_p90(const event_info_t& lhs, const event_info_t& rhs)  {
        return lhs.value_p90 < rhs.value_p90;
    }


    // desc

    static bool sort_by_timestamp_desc( const event_info_t& lhs, const event_info_t& rhs)  {
        return !(sort_by_timestamp(lhs, rhs));
    }

    static bool sort_by_tag_desc(const event_info_t& lhs, const event_info_t& rhs)  {
        return !(sort_by_tag(lhs, rhs));
    }

    static bool sort_by_count_desc(const event_info_t& lhs, const event_info_t& rhs)  {
        return !(sort_by_count(lhs, rhs));
    }

    static bool sort_by_sum_desc(const event_info_t& lhs, const event_info_t& rhs)  {
        return !(sort_by_sum(lhs, rhs));
    }

    static bool sort_by_avg_desc(const event_info_t& lhs, const event_info_t& rhs)  {
        return !(sort_by_avg(lhs, rhs));
    }

    static bool sort_by_min_desc(const event_info_t& lhs, const event_info_t& rhs)  {
        return !(sort_by_min(lhs, rhs));
    }

    static bool sort_by_max_desc(const event_info_t& lhs, const event_info_t& rhs)  {
        return !(sort_by_max(lhs, rhs));
    }

    static bool sort_by_p10_desc(const event_info_t& lhs, const event_info_t& rhs)  {
        return !(sort_by_p10(lhs, rhs));
    }

    static bool sort_by_p50_desc(const event_info_t& lhs, const event_info_t& rhs)  {
        return !(sort_by_p50(lhs, rhs));
    }

    static bool sort_by_p90_desc(const event_info_t& lhs, const event_info_t& rhs)  {
        return !(sort_by_p90(lhs, rhs));
    }
};



#endif // __BUSINESS_SORT_H__
