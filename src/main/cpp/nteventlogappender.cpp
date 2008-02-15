/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#if (defined(WIN32) || defined(_WIN32)) && !defined(_WIN32_WCE)

#include <windows.h>
#undef ERROR
#include <log4cxx/nt/nteventlogappender.h>
#include <log4cxx/spi/loggingevent.h>
#include <log4cxx/helpers/loglog.h>
#include <log4cxx/level.h>
#include <log4cxx/helpers/stringhelper.h>
#include <log4cxx/helpers/transcoder.h>
#include <log4cxx/helpers/pool.h>
#include <apr_strings.h>

using namespace log4cxx;
using namespace log4cxx::spi;
using namespace log4cxx::helpers;
using namespace log4cxx::nt;

class CCtUserSIDHelper
{
public:
        static bool FreeSid(SID * pSid)
        {
                return ::HeapFree(GetProcessHeap(), 0, (LPVOID)pSid) != 0;
        }

        static bool CopySid(SID * * ppDstSid, SID * pSrcSid)
        {
                bool bSuccess = false;

                DWORD dwLength = ::GetLengthSid(pSrcSid);
                *ppDstSid = (SID *) ::HeapAlloc(GetProcessHeap(),
                 HEAP_ZERO_MEMORY, dwLength);

                if (::CopySid(dwLength, *ppDstSid, pSrcSid))
                {
                        bSuccess = true;
                }
                else
                {
                        FreeSid(*ppDstSid);
                }

                return bSuccess;
        }

        static bool GetCurrentUserSID(SID * * ppSid)
        {
                bool bSuccess = false;

                // Pseudohandle so don't need to close it
                HANDLE hProcess = ::GetCurrentProcess();
                HANDLE hToken = NULL;
                if (::OpenProcessToken(hProcess, TOKEN_QUERY, &hToken))
                {
                        // Get the required size
                        DWORD tusize = 0;
                        GetTokenInformation(hToken, TokenUser, NULL, 0, &tusize);
                        TOKEN_USER* ptu = (TOKEN_USER*)new BYTE[tusize];

                        if (GetTokenInformation(hToken, TokenUser, (LPVOID)ptu, tusize, &tusize))
                        {
                                bSuccess = CopySid(ppSid, (SID *)ptu->User.Sid);
                        }

                        CloseHandle(hToken);
                        delete [] ptu;
                }

                return bSuccess;
        }
};

IMPLEMENT_LOG4CXX_OBJECT(NTEventLogAppender)

NTEventLogAppender::NTEventLogAppender() : hEventLog(NULL), pCurrentUserSID(NULL)
{
}

NTEventLogAppender::NTEventLogAppender(const LogString& server, const LogString& log, const LogString& source, const LayoutPtr& layout)
: server(server), log(log), source(source), hEventLog(NULL), pCurrentUserSID(NULL)
{
        this->layout = layout;
        Pool pool;
        activateOptions(pool);
}

NTEventLogAppender::~NTEventLogAppender()
{
        finalize();
}


void NTEventLogAppender::close()
{
        if (hEventLog != NULL)
        {
                ::DeregisterEventSource(hEventLog);
                hEventLog = NULL;
        }

        if (pCurrentUserSID != NULL)
        {
                CCtUserSIDHelper::FreeSid((::SID*) pCurrentUserSID);
                pCurrentUserSID = NULL;
        }
}

void NTEventLogAppender::setOption(const LogString& option, const LogString& value)
{
        if (StringHelper::equalsIgnoreCase(option, LOG4CXX_STR("SERVER"), LOG4CXX_STR("server")))
        {
                server = value;
        }
        else if (StringHelper::equalsIgnoreCase(option, LOG4CXX_STR("LOG"), LOG4CXX_STR("log")))
        {
                log = value;
        }
        else if (StringHelper::equalsIgnoreCase(option, LOG4CXX_STR("SOURCE"), LOG4CXX_STR("source")))
        {
                source = value;
        }
        else
        {
                AppenderSkeleton::setOption(option, value);
        }
}

void NTEventLogAppender::activateOptions(Pool&)
{
        if (source.empty())
        {
                LogLog::warn(
             ((LogString) LOG4CXX_STR("Source option not set for appender ["))
                + name + LOG4CXX_STR("]."));
                return;
        }

        if (log.empty())
        {
                log = LOG4CXX_STR("Application");
        }

        close();

        // current user security identifier
        CCtUserSIDHelper::GetCurrentUserSID((::SID**) &pCurrentUserSID);

        addRegistryInfo();

#if LOG4CXX_WCHAR_T_API
        LOG4CXX_ENCODE_WCHAR(wsource, source);
        LOG4CXX_ENCODE_WCHAR(wserver, server);
        hEventLog = ::RegisterEventSourceW(
            wserver.empty() ? NULL : wserver.c_str(),
            wsource.c_str());
#else
        LOG4CXX_ENCODE_CHAR(wsource, source);
        LOG4CXX_ENCODE_CHAR(wserver, server);
        hEventLog = ::RegisterEventSourceA(
            wserver.empty() ? NULL : wserver.c_str(),
            wsource.c_str());
#endif
        if (hEventLog == NULL) {
            LogString msg(LOG4CXX_STR("Cannot register NT EventLog -- server: '"));
            msg.append(server);
            msg.append(LOG4CXX_STR("' source: '"));
            msg.append(source);
            LogLog::error(msg);
            LogLog::error(getErrorString(LOG4CXX_STR("RegisterEventSource")));
        }
}

void NTEventLogAppender::append(const LoggingEventPtr& event, Pool& p)
{
        if (hEventLog == NULL)
        {
                LogLog::warn(LOG4CXX_STR("NT EventLog not opened."));
                return;
        }

        LogString oss;
        layout->format(oss, event, p);
#if LOG4CXX_WCHAR_T_API
        wchar_t* msgs = Transcoder::wencode(oss, p);
        BOOL bSuccess = ::ReportEventW(
                hEventLog,
                getEventType(event),
                getEventCategory(event),
                0x1000,
                pCurrentUserSID,
                1,
                0,
                (LPCWSTR*) &msgs,
                NULL);
#else
        char* msgs = Transcoder::encode(oss, p);
        BOOL bSuccess = ::ReportEventA(
                hEventLog,
                getEventType(event),
                getEventCategory(event),
                0x1000,
                pCurrentUserSID,
                1,
                0,
                &msgs,
                NULL);
#endif

        if (!bSuccess)
        {
                LogLog::error(getErrorString(LOG4CXX_STR("ReportEvent")));
        }
}

NTEventLogAppender::HKEY NTEventLogAppender::regGetKey(
     const LogString& subkey, DWORD *disposition)
{
        ::HKEY hkey = 0;
#if LOG4CXX_WCHAR_T_API
        LOG4CXX_ENCODE_WCHAR(wstr, subkey);
        RegCreateKeyExW((::HKEY) HKEY_LOCAL_MACHINE, wstr.c_str(), 0, NULL,
                REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL,
                &hkey, disposition);
#else
        LOG4CXX_ENCODE_CHAR(str, subkey);
        RegCreateKeyExA((::HKEY) HKEY_LOCAL_MACHINE, str.c_str(), 0, NULL,
                REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL,
                &hkey, disposition);
#endif
        return hkey;
}

void NTEventLogAppender::regSetString(HKEY hkey, const wchar_t*  name,
                                       const wchar_t* value)
{
        RegSetValueExW((::HKEY) hkey, name, 0, REG_SZ, (LPBYTE) value,
        wcslen(value)*sizeof(wchar_t));
}

void NTEventLogAppender::regSetDword(HKEY hkey, const wchar_t* name, DWORD value)
{
        RegSetValueExW((::HKEY) hkey, name, 0, REG_DWORD, (LPBYTE)&value, sizeof(DWORD));
}

/*
 * Add this source with appropriate configuration keys to the registry.
 */
void NTEventLogAppender::addRegistryInfo()
{
        DWORD disposition;
        ::HKEY hkey = 0;
        LogString subkey(LOG4CXX_STR("SYSTEM\\CurrentControlSet\\Services\\EventLog\\"));
        subkey.append(log);
        subkey.append(1, 0x5C /* '\\' */);
        subkey.append(source);

        hkey = (::HKEY) regGetKey(subkey, &disposition);
        if (disposition == REG_CREATED_NEW_KEY)
        {
                regSetString(hkey, L"EventMessageFile", L"NTEventLogAppender.dll");
                regSetString(hkey, L"CategoryMessageFile", L"NTEventLogAppender.dll");
                regSetDword(hkey,  L"TypesSupported", (DWORD)7);
                regSetDword(hkey,  L"CategoryCount", (DWORD)5);
        }

        RegCloseKey(hkey);
        return;
}

WORD NTEventLogAppender::getEventType(const LoggingEventPtr& event)
{
        WORD ret_val;

        switch (event->getLevel()->toInt())
        {
        case Level::FATAL_INT:
        case Level::ERROR_INT:
                ret_val = EVENTLOG_ERROR_TYPE;
                break;
        case Level::WARN_INT:
                ret_val = EVENTLOG_WARNING_TYPE;
                break;
        case Level::INFO_INT:
        case Level::DEBUG_INT:
        default:
                ret_val = EVENTLOG_INFORMATION_TYPE;
                break;
        }

        return ret_val;
}

WORD NTEventLogAppender::getEventCategory(const LoggingEventPtr& event)
{
        WORD ret_val;

        switch (event->getLevel()->toInt())
        {
        case Level::FATAL_INT:
                ret_val = 1;
                break;
        case Level::ERROR_INT:
                ret_val = 2;
                break;
        case Level::WARN_INT:
                ret_val = 3;
                break;
        case Level::INFO_INT:
                ret_val = 4;
                break;
        case Level::DEBUG_INT:
        default:
                ret_val = 5;
                break;
        }

        return ret_val;
}

LogString NTEventLogAppender::getErrorString(const LogString& function)
{
    Pool p;
    enum { MSGSIZE = 5000 };

#if LOG4CXX_WCHAR_T_API
    wchar_t* lpMsgBuf = (wchar_t*) p.palloc(MSGSIZE * sizeof(wchar_t));
    DWORD dw = GetLastError();

    FormatMessageW(
        FORMAT_MESSAGE_FROM_SYSTEM,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        lpMsgBuf,
        MSGSIZE, NULL );
#else
    char* lpMsgBuf = (char*) p.palloc(MSGSIZE);
    DWORD dw = GetLastError();

    FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        lpMsgBuf,
        MSGSIZE, NULL );

#endif

    LogString msg(function);
    msg.append(LOG4CXX_STR(" failed with error "));
    StringHelper::toString((size_t) dw, p, msg);
    msg.append(LOG4CXX_STR(": "));
    Transcoder::decode(lpMsgBuf, msg);

    return msg;
}

#endif // WIN32
