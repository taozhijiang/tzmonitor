cmake_minimum_required (VERSION 2.8.11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")


include_directories( ${PROJECT_SOURCE_DIR}/include/ )
include_directories( ${PROJECT_SOURCE_DIR}/source/client/include/ )

aux_source_directory(. DIR_LIB_SRCS)
add_library (tzmonitor_client STATIC ${DIR_LIB_SRCS})

install (TARGETS tzmonitor_client
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib/static )
