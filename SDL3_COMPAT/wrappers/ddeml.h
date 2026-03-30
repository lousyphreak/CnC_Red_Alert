#ifndef RA_DDEML_WRAPPER_H
#define RA_DDEML_WRAPPER_H

#include "win32_compat.h"

using HSZ = void*;
using HCONV = void*;
using HDDEDATA = void*;

struct CONVCONTEXT {
    UINT cb = 0;
};

using PCONVCONTEXT = CONVCONTEXT*;
using PFNCALLBACK = HDDEDATA (CALLBACK*)(UINT, UINT, HCONV, HSZ, HSZ, HDDEDATA, DWORD, DWORD);

#ifndef APPCLASS_STANDARD
#define APPCLASS_STANDARD 0x00000000U
#endif
#ifndef CBF_FAIL_SELFCONNECTIONS
#define CBF_FAIL_SELFCONNECTIONS 0x00001000U
#endif
#ifndef DMLERR_NO_ERROR
#define DMLERR_NO_ERROR 0U
#endif
#ifndef CP_WINANSI
#define CP_WINANSI 1004U
#endif
#ifndef DNS_REGISTER
#define DNS_REGISTER 0x0001U
#endif
#ifndef DNS_UNREGISTER
#define DNS_UNREGISTER 0x0002U
#endif
#ifndef CF_TEXT
#define CF_TEXT 1U
#endif
#ifndef XTYP_POKE
#define XTYP_POKE 0x0090U
#endif
#ifndef XTYP_REGISTER
#define XTYP_REGISTER 0x00A0U
#endif
#ifndef XTYP_UNREGISTER
#define XTYP_UNREGISTER 0x00A1U
#endif
#ifndef XTYP_ADVDATA
#define XTYP_ADVDATA 0x0010U
#endif
#ifndef XTYP_XACT_COMPLETE
#define XTYP_XACT_COMPLETE 0x0080U
#endif
#ifndef XTYP_DISCONNECT
#define XTYP_DISCONNECT 0x00C0U
#endif
#ifndef XTYP_CONNECT
#define XTYP_CONNECT 0x0060U
#endif
#ifndef DDE_FACK
#define DDE_FACK 0x8000U
#endif
#ifndef DDE_FNOTPROCESSED
#define DDE_FNOTPROCESSED 0x0000U
#endif
#ifndef SZDDESYS_TOPIC
#define SZDDESYS_TOPIC "System"
#endif

UINT DdeInitialize(LPDWORD instance_id, PFNCALLBACK callback, DWORD command, DWORD reserved);
HSZ DdeCreateStringHandle(DWORD instance_id, LPCSTR value, int code_page);
BOOL DdeFreeStringHandle(DWORD instance_id, HSZ string_handle);
BOOL DdeUninitialize(DWORD instance_id);
HDDEDATA DdeNameService(DWORD instance_id, HSZ service_name, HSZ reserved, UINT command);
HCONV DdeConnect(DWORD instance_id, HSZ service_name, HSZ topic_name, PCONVCONTEXT context);
BOOL DdeDisconnect(HCONV conversation);
HDDEDATA DdeClientTransaction(LPBYTE data, DWORD data_size, HCONV conversation, HSZ item_name, UINT format, UINT type, DWORD timeout, LPDWORD result);
DWORD DdeQueryString(DWORD instance_id, HSZ string_handle, LPSTR buffer, DWORD max_count, int code_page);
LPBYTE DdeAccessData(HDDEDATA data_handle, LPDWORD size);
BOOL DdeUnaccessData(HDDEDATA data_handle);

#endif
