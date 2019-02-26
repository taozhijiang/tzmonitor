cmake_minimum_required (VERSION 2.8.11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x " )
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Woverloaded-virtual -Wpointer-arith -Wwrite-strings -Werror=format -march=native " )

set(CMAKE_BUILD_TYPE DEBUG)
#set(CMAKE_BUILD_TYPE RELEASE)
set(CMAKE_CXX_FLAGS_DEBUG   "$ENV{CXXFLAGS} -O0 -g")
set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O2 ")


set (XTRA_RHEL6x /home/aliuser/svnclient/xtra_rhel6.x)
set (INCLUDE_DIRS
        ${PROJECT_SOURCE_DIR}/include/
        ${PROJECT_SOURCE_DIR}/source/
        ${XTRA_RHEL6x}/include/
        ${XTRA_RHEL6x}/include/google/
)

link_directories( /usr/lib64/mysql/
        ${XTRA_RHEL6x}/libs/
        ${XTRA_RHEL6x}/libs/boost/
        ${XTRA_RHEL6x}/libs/google/
        ${XTRA_RHEL6x}/libs/google/protobuf-2.5.0/
        ../lib
)

add_executable( startup startup.cpp)
add_executable( fast_report fast_report.cpp)

set (EXTRA_LIBS MonitorClient )

set (EXTRA_LIBS ${EXTRA_LIBS} ssl config++)
set (EXTRA_LIBS ${EXTRA_LIBS} pthread)
set (EXTRA_LIBS ${EXTRA_LIBS} boost_system boost_thread boost_chrono boost_regex)
set (EXTRA_LIBS ${EXTRA_LIBS} protoc protobuf )


target_link_libraries( startup -lrt -rdynamic -ldl ${EXTRA_LIBS} )
target_link_libraries( fast_report -lrt -rdynamic -ldl ${EXTRA_LIBS} )
