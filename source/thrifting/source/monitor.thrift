namespace cpp tz_thrift

struct result_t {
    1:required i32    code;       // 0: 成功; <0：失败 
    2:required string desc;
}

struct ping_t {
    1: required string msg;
}

struct ev_data_t {
    1:required string name;
    2:required string msgid;     // 消息ID，只需要在time_identity_event 域下唯一即可
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

struct ev_query_request_t {
    1:required string version;    // 1.0.0
}

struct ev_query_resp_t {
    100:required result_t result;

    1:required string version;    // 1.0.0

}

service monitor_service {

    result_t ping_test(1: ping_t req);
    result_t ev_submit(1: ev_report_t req);


}
