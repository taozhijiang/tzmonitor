cmake_minimum_required (VERSION 2.8.11)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x " )
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Woverloaded-virtual -Wpointer-arith -Wwrite-strings -Werror=format -march=native " )
include("${PROJECT_SOURCE_DIR}/help.cmake")
define_colors()


include_directories(         
    ${PROJECT_SOURCE_DIR}/include/
    ${PROJECT_SOURCE_DIR}/source/
    ${PROJECT_SOURCE_DIR}/deps/
    ${PROJECT_SOURCE_DIR}/deps/xtra_rhelz.x/include/
    ${PROJECT_SOURCE_DIR}/deps/xtra_rhelz.x/include/google/ 
)

link_directories( /usr/lib64/mysql/
    ${PROJECT_SOURCE_DIR}/deps/xtra_rhelz.x/libs/
    ${PROJECT_SOURCE_DIR}/deps/xtra_rhelz.x/libs/boost/
    ${PROJECT_SOURCE_DIR}/deps/xtra_rhelz.x/libs/google/
    ${PROJECT_SOURCE_DIR}/deps/xtra_rhelz.x/libs/google/protobuf-2.5.0/
    ../lib
)



set (TEST_HOST_LIB Scaffold Protocol RPC Network Utils HeraclesClient )

set (EXTRA_LIBS ${EXTRA_LIBS} ssl config++)
set (EXTRA_LIBS ${EXTRA_LIBS} pthread)
set (EXTRA_LIBS ${EXTRA_LIBS} boost_system boost_thread boost_chrono boost_regex)
set (EXTRA_LIBS ${EXTRA_LIBS} protoc protobuf )


set (EXTRA_LIBS ${EXTRA_LIBS} gtest gmock gtest_main)


add_individual_test(LibConfig)
add_individual_test(MessageBuffer)
add_individual_test(Protobuf)