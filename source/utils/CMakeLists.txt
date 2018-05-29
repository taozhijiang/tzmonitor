cmake_minimum_required (VERSION 2.8.11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")

set (EXTRA_CMD "cp -fr ${PROJECT_SOURCE_DIR}/source/utils/{EQueue.h,TinyTask.h,Log.h} ${PROJECT_SOURCE_DIR}/source/client/include/")
exec_program(${EXTRA_CMD})

include_directories( ${PROJECT_SOURCE_DIR}/include/ )

aux_source_directory(. DIR_LIB_SRCS)
add_library (utils STATIC ${DIR_LIB_SRCS})

install (TARGETS utils
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib/static )
