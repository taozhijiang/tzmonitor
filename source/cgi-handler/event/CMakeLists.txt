
# 控制so动态链接库中引用标准库不必要的功能
add_definitions( -DTZHTTPD_CGI_SO_MODULE )

add_library (ev_query  SHARED evquery.cpp  )