/*-
 * Copyright (c) 2018 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */


#ifndef __TTHRIFT_TYPES_H__
#define __TTHRIFT_TYPES_H__

// Thrift灵活的分层设计，每一层的可选类型比较多，为了客户端和服务端统一起见，
// 这里对各层的选项进行统一配置

#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TBufferTransports.h>

#include <thrift/transport/TSocket.h>
#include <thrift/transport/TServerSocket.h>

#if defined(BUILD_VERSION_V2)
#include <thrift/transport/TNonblockingServerSocket.h>
#endif

#include <thrift/server/TThreadedServer.h>
#include <thrift/server/TThreadPoolServer.h>
#include <thrift/server/TNonblockingServer.h>

#include <thrift/concurrency/PosixThreadFactory.h>

using namespace ::apache::thrift;

typedef protocol::TProtocolFactory          ProtocolFactory;
typedef protocol::TBinaryProtocolFactory    ProtocolTypeFactory;
typedef protocol::TBinaryProtocol           ProtocolType;

// Nonblock必须使用该类型，需要封装数据包长度信息
typedef transport::TFramedTransportFactory  TransportTypeFactory;
typedef transport::TFramedTransport         TransportType;

typedef concurrency::PosixThreadFactory     ThreadTypeFactory;


#endif // __TTHRIFT_TYPES_H__
