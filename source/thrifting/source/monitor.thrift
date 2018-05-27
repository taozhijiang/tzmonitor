namespace cpp tz_thrift

struct result_t {
    1:required i32    code;       // 0: 成功; <0：失败 
    2:required string desc;
}

struct ping_t {
    1: required string msg;
}

// 提交相关的

struct ev_data_t {
    1:required string name;
    2:required i64    msgid;     // 消息ID，只需要在time_identity_event 域下唯一即可
    3:required i64    value;
    4:required string flag;       // 标识区分，比如成功、失败、结果类别等
}

struct ev_report_t {
    1:required string version;    // 1.0.0
    2:required i64    time;
    3:required string host;       // 事件主机名或者主机IP
    4:required string serv;       // 汇报服务名
    5:required string entity_idx; // 汇报服务标识(多实例时候使用，否则默认1)

    10:required list<ev_data_t> data;  // 事件不必相同，但是必须同一个time
}

// 查询相关的

struct ev_query_request_t {
    1:required string version;          // 1.0.0
    2:required i64    interval_sec;     // 需要查询的时间长度
    3:required string name;

    5:optional i64    start;
    6:optional string host;
    7:optional string serv;
    8:optional string entity_idx;
    9:optional string flag;
    10:optional string groupby;  // none / flag / time
}

struct ev_info_t {
    1:optional i64    time;         // 如果是需要分组返回，则需要填写这个参数
    2:optional string flag;

    5:optional i32    count;
    6:optional i64    value_sum;
    7:optional i64    value_avg;
    8:optional double value_std;
}

struct ev_query_response_t {
    1:required result_t result;

    5:optional string  version;      // 1.0.0
    6:optional i64     time;         // 实际数据开始取的时间
    7:optional string  host;
    8:optional string  serv;
    9:optional string  entity_idx;

    10:optional string name;         // 如果请求条件约束，则原样返回校验
    11:optional string flag;         // 如果请求条件约束，则原样返回校验
    12:optional i64    interval_sec;

    15:optional ev_info_t summary;
    16:optional list<ev_info_t> info;
}


service monitor_service {

    result_t ping_test(1: ping_t req);

    result_t            ev_submit (1: ev_report_t req);
    ev_query_response_t ev_query  (1: ev_query_request_t req);
}
