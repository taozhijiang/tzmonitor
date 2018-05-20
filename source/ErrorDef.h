#ifndef _TZ_ERROR_DEF_H_
#define _TZ_ERROR_DEF_H_

namespace ErrorDef {

const int OK = 0;
const int NoErr = 0;

// GeneralErr
const int Error = -10001;
const int ParamErr = -10002;
const int CreateErr = -10003;
const int InitErr = -10004;
const int NotImplmented = -10005;
const int NetworkErr = -10006;
const int TimeoutErr = -10007;
const int OffServiceErr = -10008;
const int DenialOfServiceErr = -10009;


const int DatabasePoolErr = -20001;
const int DatabaseExecErr = -20002;
const int DatabaseResultErr = -20003;

const int RedisPoolErr = -30001;

const int MQPoolErr = -40001;

// DatabaseErr

// ThriftErr


}

#endif // _TZ_ERROR_DEF_H_
