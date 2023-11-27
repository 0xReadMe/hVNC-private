#include "stdafx.h"

#include <Psapi.h>
#pragma comment (lib, "Psapi.lib")
#include <WinSock2.h>
#include <WS2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#include <Shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

#include <commontoolsimpl.h>
#include <commonsocket.h>
#include "settingsimpl.h"
#include "dbimpl.h"
#include "socksimpl.h"
#include "serverimpl.h"

#ifdef WINDOWS_SERVICE
#include "winserviceimpl.h"
#endif // WINDOWS_SERVICE

bool GetSettings(TServerSettings& settings)
{
#ifdef WINDOWS_SERVICE

	return WinService::GetSettings(settings);

#else // WINDOWS_SERVICE

#error Only Windows service configuration is supported.

#endif // WINDOWS_SERVICE
}

bool GetDBSettings(TDBSettings& settings)
{
#ifdef WINDOWS_SERVICE

	return WinService::GetDBSettings(settings);

#else // WINDOWS_SERVICE

#error Only Windows service configuration is supported.

#endif // WINDOWS_SERVICE
}

int _tmain(int argc, _TCHAR* argv[])
{
#ifdef WINDOWS_SERVICE

	if (WinService::ProcessCommandLine(argc, argv))
		return 0;

	SERVICE_TABLE_ENTRY ServiceTable[2];
	ServiceTable[0].lpServiceName = SERVICE_NAME;
	ServiceTable[0].lpServiceProc = WinService::ServiceMain;
	ServiceTable[1].lpServiceName = NULL;
	ServiceTable[1].lpServiceProc = NULL;
	StartServiceCtrlDispatcher(ServiceTable);

#else // WINDOWS_SERVICE

	#error Only Windows service configuration is supported.

#endif // WINDOWS_SERVICE

	return 0;
}

