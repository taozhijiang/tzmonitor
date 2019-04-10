cmake_minimum_required (VERSION 2.8.11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x " )
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Woverloaded-virtual -Wpointer-arith -Wwrite-strings -Werror=format -march=native " )

set(CMAKE_BUILD_TYPE DEBUG)
#set(CMAKE_BUILD_TYPE RELEASE)
set(CMAKE_CXX_FLAGS_DEBUG   "$ENV{CXXFLAGS} -O0 -g")
set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O2 ")


set (INCLUDE_DIRS
        ${PROJECT_SOURCE_DIR}/include/
        ${PROJECT_SOURCE_DIR}/source/
        ${PROJECT_SOURCE_DIR}/deps/
        ${PROJECT_SOURCE_DIR}/deps/xtra_rhelz.x/include/
        ${PROJECT_SOURCE_DIR}/deps/xtra_rhelz.x/include/google/
)
include_directories( ${INCLUDE_DIRS} )

link_directories( /usr/lib64/mysql/
        ${PROJECT_SOURCE_DIR}/deps/xtra_rhelz.x/libs/
        ${PROJECT_SOURCE_DIR}/deps/xtra_rhelz.x/libs/boost/
        ${PROJECT_SOURCE_DIR}/deps/xtra_rhelz.x/libs/google/
        ${PROJECT_SOURCE_DIR}/deps/xtra_rhelz.x/libs/google/protobuf-2.5.0/
        ../lib
)

add_subdirectory( ../deps/tzhttpd build )

add_executable( startup startup.cpp)
add_executable( fast_report fast_report.cpp)
add_executable( http_face stat_handler.cpp http_face.cpp )

set (EXTRA_LIBS HeraclesClient )

set (EXTRA_LIBS ${EXTRA_LIBS} ssl crypto config++)
set (EXTRA_LIBS ${EXTRA_LIBS} pthread)
set (EXTRA_LIBS ${EXTRA_LIBS} boost_system boost_thread boost_chrono boost_regex)
set (EXTRA_LIBS ${EXTRA_LIBS} protoc protobuf )


target_link_libraries( startup -lrt -rdynamic -ldl ${EXTRA_LIBS} )
target_link_libraries( fast_report -lrt -rdynamic -ldl ${EXTRA_LIBS} )
target_link_libraries( http_face -lrt -rdynamic -ldl tzhttpd ${EXTRA_LIBS} cryptopp )

