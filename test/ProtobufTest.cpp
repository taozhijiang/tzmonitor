#include <iostream>
#include <string>

#include <gmock/gmock.h>
using namespace ::testing;

#include <other/Log.h>
#include <message/ProtoBuf.h>
#include <Protocol/gen-cpp/MonitorTask.pb.h>

using namespace tzrpc;

#if 0
message XtraReadOps {

    // 所有请求报文
    message Request {

        message Get {
            required string key = 1;
        }
        optional Get gets = 3;

        message Ping {
            required string msg = 1;
        }
        optional Ping ping = 4;

    }
}
#endif


TEST(ProtobufTest, MarshalandUnmarshalTest) {

    MonitorTask::MonitorReadOps::Request request;

    ASSERT_THAT(request.IsInitialized(), Eq(true));  // all optional

    request.mutable_ping()->set_msg("nicol");
    ASSERT_THAT(request.IsInitialized(), Eq(true));

    ASSERT_THAT(request.ping().msg(), Eq("nicol"));

    request.mutable_ping()->set_msg("ping_msg");

    roo::log_info("full protobuf dump message:\n%s", roo::ProtoBuf::dump(request).c_str());
    std::cout << "full protobuf dump message:\n" << roo::ProtoBuf::dump(request) << std::endl;

    // copy
    auto copyMsg = roo::ProtoBuf::copy(request);
    ASSERT_THAT(*copyMsg, Eq(request));

    ASSERT_THAT(*copyMsg, Eq(request));

    // marshal & unmarshal

    std::string mar_str;
    ASSERT_TRUE(roo::ProtoBuf::marshalling_to_string(request, &mar_str));
    std::cout << "marshal size: " << mar_str.size() << std::endl;

    decltype(request) new_msg;
    ASSERT_TRUE(roo::ProtoBuf::unmarshalling_from_string(mar_str, &new_msg));

    ASSERT_THAT(request == new_msg, Eq(true));

    roo::log_info("full new_msg dump message:\n%s", roo::ProtoBuf::dump(new_msg).c_str());
    std::cout << "full new_msg dump message:\n" << roo::ProtoBuf::dump(new_msg) << std::endl;
}
