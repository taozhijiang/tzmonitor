/*-
 * Copyright (c) 2018 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */


#ifndef _TZ_ERROR_DEF_H_
#define _TZ_ERROR_DEF_H_

namespace ErrorDef {

const int OK = 0;
const int NoErr = 0;

// GeneralErr
const int Error = -10001;
const int ParamErr = -10002;
const int CheckErr = -10003;
const int CreateErr = -10004;
const int InitErr = -10005;
const int NotImplmented = -10006;
const int NetworkErr = -10007;
const int TimeoutErr = -10008;
const int OffServiceErr = -10009;
const int DenialOfServiceErr = -10010;
const int MsgOld = -10011;

// DatabaseErr
const int DatabasePoolErr = -20001;
const int DatabaseExecErr = -20002;
const int DatabaseResultErr = -20003;

const int RedisPoolErr = -30001;


const int MQPoolErr = -40001;

// ThriftErr
const int ThriftErr = -50001;

const int HttpErr = -60001;

}

#endif // _TZ_ERROR_DEF_H_
