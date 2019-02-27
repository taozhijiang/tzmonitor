



CREATE TABLE `t_tzmonitor_[service]_events_2019xx` (
  `F_increment_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `F_timestamp` bigint(20) NOT NULL COMMENT '事件上报时间， FROM_UNIXTIME可视化',

  `F_entity_idx` varchar(128) NOT NULL DEFAULT '' COMMENT '多服务实例编号',
  `F_metric` varchar(128) NOT NULL COMMENT '事件名称',
  `F_tag` varchar(128) NOT NULL DEFAULT 'T' COMMENT '事件结果分类',

  `F_step` int(11) NOT NULL DEFAULT '1' COMMENT '事件合并的时间间隔',
  `F_count` int(11) NOT NULL COMMENT '当前间隔内事件总个数',
  `F_value_sum` bigint(20) NOT NULL COMMENT '数值累计',
  `F_value_avg` bigint(20) NOT NULL COMMENT '数值均值',
  `F_value_std` bigint(20) NOT NULL COMMENT '数值方差',
  `F_value_min` bigint(20) NOT NULL COMMENT 'MIN',
  `F_value_max` bigint(20) NOT NULL COMMENT 'MAX',
  `F_value_p50` bigint(20) NOT NULL COMMENT 'P50',
  `F_value_p90` bigint(20) NOT NULL COMMENT 'P90',
  `F_update_time` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`F_increment_id`),
  KEY `F_index` (`F_timestamp`, `F_metric`, `F_tag`)
) ENGINE=InnoDB AUTO_INCREMENT=1 DEFAULT CHARSET=utf8


service_entityidx -> timestamp_metric_tag


// leveldb 存储表设计思路
// tzmonitor/tzmonitor__service__events_201902
//           key: metric#timestamp#tag#entity_idx
//           val: step#count#sum#avg#std#min#max#p50#p90

关于leveldb的时间，目前unixstamp已经占用的长度是10位，而要超过10位还需要很久很久
leveldb的默认顺序是升序，但是时间序列最常见的应用是降序获取最新数据，所以这里在
底层存储时间戳的时候使用 9999999999-now的方式存储，就不用折腾比较器了
