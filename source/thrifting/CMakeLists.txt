cmake_minimum_required (VERSION 2.8.11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")


include_directories( ${PROJECT_SOURCE_DIR}/include/ )

aux_source_directory(gen-cpp DIR_LIB_SRCS)
aux_source_directory(helper  DIR_LIB_SRCS)
aux_source_directory(biz     DIR_LIB_SRCS)
add_library (thrifting STATIC ${DIR_LIB_SRCS})

install (TARGETS thrifting
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib/static )
