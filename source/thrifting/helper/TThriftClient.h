#ifndef __TTHRIFT_CLIENT_H__
#define __TTHRIFT_CLIENT_H__

#include <memory>
#include <utility>
#include <functional>
#include <sstream>

#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>

#include "TThriftTypes.h"
#include <utils/Log.h>

#include "../biz/TzMonitorService.h"

// 客户端相对比较的单一，所以相对服务端也比较简单；
// 同时为了方便起见，客户端和服务端采用默认的短连接通信模式；

class TThriftClient: public boost::noncopyable {

public:
    template <typename ClientHandler, typename Callable, class... Args>
    static int call_service(std::string ip, uint16_t port, const Callable& func, Args&&... args ) {
        typedef boost::optional<std::shared_ptr<ClientHandler>> OptionalClient;
        OptionalClient op_client = create_client<ClientHandler>(ip, port);
        if (!op_client) {
            return -1;
        }

        // std::function<void()> f = std::bind(func, *client, std::forward<Args>(args) ...);
        // f();

        // 成员函数指针，此处不能使用智能指针
        std::shared_ptr<ClientHandler> client = *op_client;
        ((client.get())->*func)(std::forward<Args>(args) ...);

        return 0;
    }

    template <typename ClientHandler, typename Callable, class... Args>
    static int call_service(const std::shared_ptr<ClientHandler>& client, const Callable& func, Args&&... args ) {
        if (!client) {
            return -1;
        }

        // std::function<void()> f = std::bind(func, *client, std::forward<Args>(args) ...);
        // f();

        // 成员函数指针，此处不能使用智能指针
        ((client.get())->*func)(std::forward<Args>(args) ...);
        return 0;
    }


    template <typename ClientHandler>
    static boost::optional<std::shared_ptr<ClientHandler>>
           create_client(std::string ip, uint16_t port, int timeout_ms = 0 /*ms*/) {

        typedef boost::optional<std::shared_ptr<ClientHandler>> OptionalClient;
        OptionalClient ret;

        try {
            boost::shared_ptr<transport::TSocket> socket = boost::make_shared<transport::TSocket>(ip, port);
            if (timeout_ms) {
                socket->setConnTimeout(timeout_ms);
                socket->setRecvTimeout(timeout_ms);
                socket->setSendTimeout(timeout_ms);
            }

            boost::shared_ptr<TransportType> transport = boost::make_shared<TransportType>(socket);
            transport->open();
            boost::shared_ptr<ProtocolType>  protocol  = boost::make_shared<ProtocolType>(transport);

            std::shared_ptr<ClientHandler> client (new ClientHandler(protocol));

            ret = client;
        } catch (...) {
            log_err("Exception caught when create client: %s:%d", ip.c_str(), port);
        }

        return ret;
    }

};

#endif // __TTHRIFT_CLIENT_H__
