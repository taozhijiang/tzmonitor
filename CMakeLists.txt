cmake_minimum_required (VERSION 2.8.11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x " )
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wconversion -Woverloaded-virtual -Wpointer-arith -Wshadow -Wwrite-strings -march=native " )

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_FLAGS_DEBUG   "$ENV{CXXFLAGS} -O0 -g")
set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O2 ")

set (TZ_VERSION_MAJOR 0)
set (TZ_VERSION_MINOR 0)
set (TZ_VERSION_PATCH 1)
configure_file (
  "include/config.h.in"
  "../include/config.h" )

# mysqlcppconn如果使用C开发会在释放的时候段错误

add_definitions (-DBOOST_LOG_DYN_LINK)
aux_source_directory( source/ DIR_SRCS )

include_directories( include/ )
include_directories( source/ )
include_directories( /usr/include/mysql )

add_subdirectory( source/json )
add_subdirectory( source/utils )
add_subdirectory( source/connect )
add_subdirectory( source/httpd )
add_subdirectory( source/module )


link_directories( /usr/lib64/mysql/ )

exec_program( "export BUILD_VAR=`git log -1 --pretty=%H` && echo 'const char *build_commit_version = \"VCS: Commit:' $BUILD_VAR '\";' > build_version.cpp ")
exec_program( "export BUILD_VAR=`git symbolic-ref HEAD` && echo 'const char *build_commit_branch = \"VCS: Branch:' $BUILD_VAR '\";' >> build_version.cpp ")
exec_program( "export BUILD_VAR=`git log -1 --pretty=%cd` && echo 'const char *build_commit_date = \"VCS: Date:' $BUILD_VAR '\";' >> build_version.cpp ")
exec_program( "export BUILD_VAR=`git log -1 --pretty=\"%an %ae\"` && echo 'const char *build_commit_author = \"VCS: Author:' $BUILD_VAR '\";' >> build_version.cpp ")
exec_program( "export BUILD_VAR=`date` && echo 'const char *build_time = \"Build At:' $BUILD_VAR '\";' >> build_version.cpp ")

add_executable( tzmonitor main.cpp build_version.cpp ${DIR_SRCS} )

# ld iconv ?
set (EXTRA_LIBS ${EXTRA_LIBS} ssl event config++)
set (EXTRA_LIBS ${EXTRA_LIBS} pthread hiredis)
set (EXTRA_LIBS ${EXTRA_LIBS} boost_system boost_thread-mt boost_date_time boost_regex)
set (EXTRA_LIBS ${EXTRA_LIBS} mysqlcppconn)
set (EXTRA_LIBS ${EXTRA_LIBS} curl rabbitmq json utils connect httpd module)

target_link_libraries( tzmonitor ${EXTRA_LIBS} )

# /usr/local/bin/netask
install (TARGETS tzmonitor DESTINATION bin )
# /usr/local/include/netask/
install (DIRECTORY include/ DESTINATION include/tzmonitor)


# build a CPack driven installer package
include (InstallRequiredSystemLibraries)
set (CPACK_PACKAGE_VERSION_MAJOR "${tz_VERSION_MAJOR}")
set (CPACK_PACKAGE_VERSION_MINOR "${tz_VERSION_MINOR}")
set (CPACK_PACKAGE_VERSION_PATCH "${tz_VERSION_PATCH}")
include (CPack)
