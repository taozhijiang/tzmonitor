cmake_minimum_required (VERSION 2.8.11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x " )
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wconversion -Woverloaded-virtual -Wpointer-arith -Wshadow -Wwrite-strings -march=native " )

# if what to see debug in verbose mode
# cmake -DCMAKE_VERBOSE_MAKEFILE=true ../

set(CMAKE_BUILD_TYPE DEBUG)
#set(CMAKE_BUILD_TYPE RELEASE)
set(CMAKE_CXX_FLAGS_DEBUG   "$ENV{CXXFLAGS} -O0 -g")
set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O2 ")

# 是否编译unit_test，因为single-header模式，所以速度比较慢
# set(TZ_BUILD_UNIT_TEST TRUE)

set (TZ_VERSION_MAJOR 1)
set (TZ_VERSION_MINOR 0)
set (TZ_VERSION_PATCH 0)
configure_file (
  "include/config.h.in"
  "../include/config.h" )

# mysqlcppconn如果使用C开发会在释放的时候段错误

set (THRIFT_GEN_CMD "cd ${PROJECT_SOURCE_DIR}/source/thrifting/source && rm -fr ../gen-cpp && mkdir -p ../gen-cpp && make ")
exec_program(${THRIFT_GEN_CMD})

aux_source_directory( source/ DIR_SRCS )

include_directories( include/
                     source/
                     source/client/include
					 xtra_rhel6.x/include
                     /usr/include/mysql )

add_subdirectory( source/utils )
add_subdirectory( source/connect )
add_subdirectory( source/tzhttpd )
add_subdirectory( source/module )
add_subdirectory( source/thrifting )
add_subdirectory( source/core)
add_subdirectory( source/client)

add_subdirectory( source/cgi-handler/event )

IF(CMAKE_BUILD_TYPE MATCHES DEBUG)
    message("DEBUG MODE, using debug librarys ... ")
	link_directories( /usr/lib64/mysql/
				  ${PROJECT_SOURCE_DIR}/xtra_rhel6.x/libs/debug
				  ${PROJECT_SOURCE_DIR}/xtra_rhel6.x/libs/debug/boost
				  ${PROJECT_SOURCE_DIR}/xtra_rhel6.x/libs/debug/event2
				  ${PROJECT_SOURCE_DIR}/xtra_rhel6.x/libs/debug/thrift
				)
ELSE(CMAKE_BUILD_TYPE MATCHES DEBUG)
    message("RELEASE MODE, using release librarys ... ")
	link_directories( /usr/lib64/mysql/
				  ${PROJECT_SOURCE_DIR}/xtra_rhel6.x/libs/release
				  ${PROJECT_SOURCE_DIR}/xtra_rhel6.x/libs/release/boost
				  ${PROJECT_SOURCE_DIR}/xtra_rhel6.x/libs/release/event2
				  ${PROJECT_SOURCE_DIR}/xtra_rhel6.x/libs/release/thrift
				)
ENDIF(CMAKE_BUILD_TYPE MATCHES DEBUG)

# local temp generated file
exec_program( "export BUILD_VAR=`git log -1 --pretty=%H` && echo 'const char *build_commit_version = \"VCS: Commit:' $BUILD_VAR '\";' > build_version.cpp ")
exec_program( "export BUILD_VAR=`git symbolic-ref HEAD` && echo 'const char *build_commit_branch = \"VCS: Branch:' $BUILD_VAR '\";' >> build_version.cpp ")
exec_program( "export BUILD_VAR=`git log -1 --pretty=%cd` && echo 'const char *build_commit_date = \"VCS: Date:' $BUILD_VAR '\";' >> build_version.cpp ")
exec_program( "export BUILD_VAR=`git log -1 --pretty=\"%an %ae\"` && echo 'const char *build_commit_author = \"VCS: Author:' $BUILD_VAR '\";' >> build_version.cpp ")
exec_program( "export BUILD_VAR=`date` && echo 'const char *build_time = \"Build At:' $BUILD_VAR '\";' >> build_version.cpp ")

add_executable( tzmonitor main.cpp build_version.cpp ${DIR_SRCS} )

# ld iconv ?

set (EXTRA_LIBS ${EXTRA_LIBS} core tzhttpd utils connect module thrifting jsoncpp tinyxml)
set (EXTRA_LIBS ${EXTRA_LIBS} rabbitmq ssl curl event_core config++)
set (EXTRA_LIBS ${EXTRA_LIBS} pthread hiredis)
set (EXTRA_LIBS ${EXTRA_LIBS} boost_system boost_thread boost_date_time boost_regex)
set (EXTRA_LIBS ${EXTRA_LIBS} mysqlcppconn)
set (EXTRA_LIBS ${EXTRA_LIBS} libthriftnb.a libthriftz.a libthrift.a )

target_link_libraries( tzmonitor ${EXTRA_LIBS} )

IF(TZ_BUILD_UNIT_TEST)
    message("build unit_test ... ")
	
	# unit_test part
	enable_testing()
	# add_definitions(-DBOOST_TEST_DYN_LINK)
	include(BoostTestHelper.cmake)
	file(GLOB UNIT_TEST_SRCS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} test/*.cpp)

	foreach(test_source_file ${UNIT_TEST_SRCS})
		add_boost_test( ${test_source_file} ${EXTRA_LIBS} )
	endforeach(test_source_file)

ENDIF(TZ_BUILD_UNIT_TEST)

# /usr/local/bin/
install (TARGETS tzmonitor DESTINATION bin )
# /usr/local/include/
install (DIRECTORY include/ DESTINATION include/tzmonitor)


# build a CPack driven installer package
include (InstallRequiredSystemLibraries)
set (CPACK_PACKAGE_VERSION_MAJOR "${tz_VERSION_MAJOR}")
set (CPACK_PACKAGE_VERSION_MINOR "${tz_VERSION_MINOR}")
set (CPACK_PACKAGE_VERSION_PATCH "${tz_VERSION_PATCH}")
include (CPack)
