/*-
 * Copyright (c) 2019 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */


#include <sstream>

#include <Connect/SqlConn.h>

namespace tzrpc {

SqlConn::SqlConn(ConnPool<SqlConn, SqlConnPoolHelper>& pool, const SqlConnPoolHelper& helper):
    driver_(),
    stmt_(),
    pool_(pool),
    helper_(helper) {
}

bool SqlConn::init(int64_t conn_uuid) {

    try {

        driver_ = get_driver_instance(); // not thread safe!!!

        std::stringstream output;
        output << "# " << driver_->getName() << ", version ";
        output << driver_->getMajorVersion() << "." << driver_->getMinorVersion();
        output << "." << driver_->getPatchVersion() << endl;
        log_info("Driver info: %s", output.str().c_str());

        sql::ConnectOptionsMap connection_properties;
        connection_properties["hostName"] = helper_.host_;
        connection_properties["port"] = helper_.port_;
        connection_properties["userName"] = helper_.user_;
        connection_properties["password"] = helper_.passwd_;
        connection_properties["database"] = helper_.db_;

        int timeout_val = 600000;
        connection_properties["OPT_RECONNECT"] = true;
        connection_properties["OPT_CONNECT_TIMEOUT"] = timeout_val;
        connection_properties["OPT_READ_TIMEOUT"] = timeout_val;
        connection_properties["OPT_WRITE_TIMEOUT"] = timeout_val;

        connection_properties["CLIENT_MULTI_STATEMENTS"] = true;
        connection_properties["MYSQL_SET_CHARSET_NAME"] = helper_.charset_;

        conn_.reset(driver_->connect(connection_properties));
        stmt_.reset(conn_->createStatement());
    }
    catch (sql::SQLException &e) {

        std::stringstream output;
        output << "# ERR: " << e.what() << endl;
        output << " (MySQL error code: " << e.getErrorCode() << endl;
        output << ", SQLState: " << e.getSQLState() << " )" << endl;
        log_err("%s", output.str().c_str());
        return false;
    }

    stmt_->execute("USE " + helper_.db_ + ";");
    conn_uuid_ = conn_uuid;
    log_info("Create New SQL Connection OK! UUID: %lx", conn_uuid);
    return true;
}

SqlConn::~SqlConn() {

    /* reset to fore delete, actually not need */
    conn_.reset();
    stmt_.reset();

    log_info("Destroy Sql Connection OK!");
}

bool SqlConn::ping_test() {

    std::string sql = "show databases;";

    shared_result_ptr result;
    result.reset(sqlconn_execute_query(sql));
    if (!result) {
        log_err("Failed to query info: %s", sql.c_str());
        return false;
    }

    return true;
}

bool SqlConn::sqlconn_execute(const string& sql) {

    try {

        if(!conn_->isValid()) {
            log_err("Invalid connect, do re-connect...");
            conn_->reconnect();
        }

        stmt_->execute(sql);
        return true;

    } catch (sql::SQLException &e) {

        std::stringstream output;
        output << " STMT: " << sql << endl;
        output << "# ERR: " << e.what() << endl;
        output << " (MySQL error code: " << e.getErrorCode() << endl;
        output << ", SQLState: " << e.getSQLState() << " )" << endl;
        log_err("%s", output.str().c_str());
    }

    return false;
}

sql::ResultSet* SqlConn::sqlconn_execute_query(const string& sql) {

    sql::ResultSet* result = NULL;

    try {

        if(!conn_->isValid()) {
            log_err("Invalid connect, do re-connect...");
            conn_->reconnect();
        }

        stmt_->execute(sql);
        result = stmt_->getResultSet();

    } catch (sql::SQLException &e) {

        std::stringstream output;
        output << " STMT: " << sql << endl;
        output << "# ERR: " << e.what() << endl;
        output << " (MySQL error code: " << e.getErrorCode() << endl;
        output << ", SQLState: " << e.getSQLState() << " )" << endl;
        log_err("%s", output.str().c_str());

    }

    return result;
}

int SqlConn::sqlconn_execute_update(const string& sql) {

    try {

        if(!conn_->isValid()) {
            log_err("Invalid connect, do re-connect...");
            conn_->reconnect();
        }

        return stmt_->executeUpdate(sql);

    } catch (sql::SQLException &e) {

        std::stringstream output;
        output << " STMT: " << sql << endl;
        output << "# ERR: " << e.what() << endl;
        output << " (MySQL error code: " << e.getErrorCode() << endl;
        output << ", SQLState: " << e.getSQLState() << " )" << endl;
        log_err("%s", output.str().c_str());

    }

    return -1;
}



} // end namespace tzrpc
