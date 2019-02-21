



CREATE TABLE `t_tzmonitor_[service]_events_2019xx` (
  `F_increment_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `F_timestamp` bigint(20) NOT NULL COMMENT '事件上报时间， FROM_UNIXTIME可视化',

  `F_metric` varchar(128) NOT NULL COMMENT '事件名称',
  `F_tag` varchar(128) NOT NULL DEFAULT 'T' COMMENT '事件结果分类',

  `F_entity_idx` varchar(128) NOT NULL DEFAULT '1' COMMENT '多服务实例编号',
  `F_step` int(11) NOT NULL DEFAULT '1' COMMENT '事件合并的时间间隔',
  `F_count` int(11) NOT NULL COMMENT '当前间隔内事件总个数',
  `F_value_sum` bigint(20) NOT NULL COMMENT '数值累计',
  `F_value_avg` bigint(20) NOT NULL COMMENT '数值均值',
  `F_value_std` double NOT NULL COMMENT '数值方差',
  `F_update_time` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`F_increment_id`),
  KEY `F_index` (`F_timestamp`, `F_metric`, `F_tag`)
) ENGINE=InnoDB AUTO_INCREMENT=1 DEFAULT CHARSET=utf8


service -> timestamp_metric_tag_entityidx
