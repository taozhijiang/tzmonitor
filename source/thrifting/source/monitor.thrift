namespace cpp tz_thrift

struct result_t {
    1:required i32    code;       // 0: 成功; <0：失败 
    2:required string desc;
}

struct ping_t {
    1: required string msg;
}

service monitor_service {

    result_t ping_test(1: ping_t req);

}
