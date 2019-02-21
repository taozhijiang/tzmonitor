cmake_minimum_required (VERSION 2.8.11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")

set (EXTRA_CMD "cp -fr ${PROJECT_SOURCE_DIR}/source/Business/EventTypes.h ${PROJECT_SOURCE_DIR}/source/Client/include/ ")
exec_program(${EXTRA_CMD})

aux_source_directory(. DIR_LIB_SRCS)
add_library (Business STATIC ${DIR_LIB_SRCS})

install (TARGETS Business 
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib/static )
