cmake_minimum_required (VERSION 2.8.11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")

set (EXTRA_CMD "cp -fr ${PROJECT_SOURCE_DIR}/source/core/EventTypes.h ${PROJECT_SOURCE_DIR}/include/client/ ")
exec_program(${EXTRA_CMD})

aux_source_directory(. DIR_LIB_SRCS)
add_library (core STATIC ${DIR_LIB_SRCS})

install (TARGETS core 
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib/static )
