cmake_minimum_required (VERSION 2.8.11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")


include_directories( ${PROJECT_SOURCE_DIR}/include/ )

aux_source_directory(. DIR_LIB_SRCS)

IF(LIBEVENT_VERSION STREQUAL "event")
	list(APPEND DIR_LIB_SRCS "detail/TSv1.cpp")
ELSEIF(LIBEVENT_VERSION STREQUAL "event2")
	list(APPEND DIR_LIB_SRCS "detail/TSv2.cpp")
ELSE(LIBEVENT_VERSION STREQUAL "event")
	message(FATAL_ERROR "Unknown LIBEVENT_VERSION specified")
ENDIF(LIBEVENT_VERSION STREQUAL "event")

add_library (module STATIC ${DIR_LIB_SRCS})
install (TARGETS module
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib/static )
