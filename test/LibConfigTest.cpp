#include <gmock/gmock.h>
#include <string>

using namespace ::testing;

#include <scaffold/Setting.h>
#include <string/StrUtil.h>

using namespace roo;

TEST(LibConfigTest, SysConfigInitVefifyTest) {

#if 0
    std::string cfgFile = "../heracles_example.conf";

    bool b_ret = ConfHelper::instance().init(cfgFile);
    ASSERT_TRUE(b_ret);

    auto conf_ptr = ConfHelper::instance().get_conf();
    ASSERT_TRUE(conf_ptr);

    std::string s_value;
    int         i_value;

    conf_ptr->lookupValue("version", s_value);
    ASSERT_THAT(s_value, Eq("3.0.0"));

    conf_ptr->lookupValue("log_level", i_value);
    ASSERT_THAT(i_value, Eq(7));

#endif

}
