#ifndef _TiBANK_CONN_IF_HPP_
#define _TiBANK_CONN_IF_HPP_

#include <utils/Log.h>

enum ConnStat {
    kConnWorking = 1,
    kConnPending,
    kConnError,
};

enum ShutdownType {
    kShutdownSend = 1,
    kShutdownRecv = 2,
    kShutdownBoth = 3,
};

class ConnIf {

public:

    /// Construct a connection with the given socket.
    explicit ConnIf(std::shared_ptr<ip::tcp::socket> sock_ptr):
        conn_stat_(kConnPending), sock_ptr_(sock_ptr) {
        set_tcp_nonblocking(false);
    }

    virtual ~ConnIf() {}

public:
    // some general tiny function
    // some general tiny settings function

    bool set_tcp_nonblocking(bool set_value) {
        socket_base::non_blocking_io command(set_value);
        sock_ptr_->io_control(command);

        return true;
    }

    bool set_tcp_nodelay(bool set_value) {

        boost::asio::ip::tcp::no_delay nodelay(set_value);
        sock_ptr_->set_option(nodelay);
        boost::asio::ip::tcp::no_delay option;
        sock_ptr_->get_option(option);

        return (option.value() == set_value);
    }

    bool set_tcp_keepalive(bool set_value) {

        boost::asio::socket_base::keep_alive keepalive(set_value);
        sock_ptr_->set_option(keepalive);
        boost::asio::socket_base::keep_alive option;
        sock_ptr_->get_option(option);

        return (option.value() == set_value);
    }

    void sock_shutdown(enum ShutdownType s) {

        boost::system::error_code ignore_ec;
        if (s == kShutdownSend) {
            sock_ptr_->shutdown(boost::asio::socket_base::shutdown_send, ignore_ec);
        } else if (s == kShutdownRecv) {
            sock_ptr_->shutdown(boost::asio::socket_base::shutdown_receive, ignore_ec);
        } else if (s == kShutdownBoth) {
            sock_ptr_->shutdown(boost::asio::socket_base::shutdown_both, ignore_ec);
        }
    }

    void sock_cancel() {
        boost::system::error_code ignore_ec;
        sock_ptr_->cancel(ignore_ec);
    }

    void sock_close() {
        boost::system::error_code ignore_ec;
        sock_ptr_->close(ignore_ec);
    }

    enum ConnStat get_conn_stat() { return conn_stat_; }
    void set_conn_stat(enum ConnStat stat) { conn_stat_ = stat; }

private:
    enum ConnStat conn_stat_;

protected:
    std::shared_ptr<ip::tcp::socket> sock_ptr_;
};


typedef std::shared_ptr<ConnIf> ConnIfPtr;
typedef std::weak_ptr<ConnIf>   ConnIfWeakPtr;

#endif //_TiBANK_CONN_IF_HPP_
