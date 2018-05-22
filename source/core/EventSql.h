#ifndef _TZ_EVENT_SQL_H__
#define _TZ_EVENT_SQL_H__

#include "General.h"

#include <connect/SqlConn.h>
#include "EventItem.h"

#if 0
CREATE TABLE `t_tzmonitor_event_stat_201805` (

  `F_id` int(11) NOT NULL AUTO_INCREMENT,

  `F_host` varchar(128) NOT NULL COMMENT '事件来源主机',
  `F_serv` varchar(128) NOT NULL COMMENT '事件来源服务',
  `F_entity_idx` varchar(128) NOT NULL DEFAULT '1' COMMENT '多服务实例编号',
  `F_time` bigint(20) NOT NULL COMMENT '事件上报时间， FROM_UNIXTIME可视化',

  `F_name` varchar(128) NOT NULL COMMENT '事件名称',
  `F_flag` varchar(128) NOT NULL DEFAULT 'T' COMMENT '事件结果分类',
  `F_count` int(11) NOT NULL COMMENT '当前秒钟事件个数',
  `F_value_sum` bigint(20) NOT NULL COMMENT '数值累计',
  `F_value_avg` bigint(20) NOT NULL COMMENT '数值均值',
  `F_value_std` double NOT NULL COMMENT '数值方差',

  `F_update_time` TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,

  PRIMARY KEY (`F_id`),
  KEY `F_time` (`F_time`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8

#endif


namespace EventSql {

extern std::string database;
extern std::string table_prefix;


int insert_ev_stat(const event_insert_t& stat);
int insert_ev_stat(sql_conn_ptr &conn, const event_insert_t& stat);

// group
int query_ev_stat(const event_cond_t& cond, event_query_t& stat);
int query_ev_stat(sql_conn_ptr& conn, const event_cond_t& cond, event_query_t& stat);


} // end namespace

#endif // _TZ_EVENT_SQL_H__
