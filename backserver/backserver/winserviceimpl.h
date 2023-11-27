#pragma once

#define SERVICE_NAME _T("backserver")
#define SERVICE_INFO _T("Backconnect server")

#define SETTINGS_FILENAME			_T("backserver.ini")
#define SETTINGS_SERVER				_T("server")
#define SETTINGS_SERVER_PORT		_T("port")
#define SETTINGS_SERVER_ACCESS		_T("access")
#define SERVER_DEFAULT_PORT 9999

#define SETTINGS_DB_FILENAME		"backserver.ini"
#define SETTINGS_DB					"DB"
#define SETTINGS_DB_USER			"user"
#define SETTINGS_DB_PASS			"password"
#define SETTINGS_DB_DATA			"database"
#define SETTINGS_DB_SERV			"server"
#define SETTINGS_DB_PORT			"port"

#define DB_DEFAULT_USER				"root"
#define DB_DEFAULT_PASS				""
#define DB_DEFAULT_DATA				"backserver"
#define DB_DEFAULT_SERV				"localhost"
#define DB_DEFAULT_PORT				3306

namespace WinService
{
	bool Uninstall()
	{
		SC_HANDLE hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
		if (NULL == hSCM)
			return false;

		SC_HANDLE hService = OpenService(hSCM, SERVICE_NAME, SERVICE_STOP | DELETE | SERVICE_QUERY_STATUS);
		CloseServiceHandle(hSCM);

		bool bResult = false;
		if (hService)
		{
			SERVICE_STATUS Status;
			if (QueryServiceStatus(hService, &Status))
			{
				while (SERVICE_STOPPED != Status.dwCurrentState)
				{
					ControlService(hService, SERVICE_CONTROL_STOP, &Status);
					Sleep(Status.dwWaitHint ? Status.dwWaitHint : 100);
				}

				bResult = (FALSE != DeleteService(hService));
			}

			CloseServiceHandle(hService);
		}

		return bResult;
	}

	bool Install()
	{
		Uninstall();

		SC_HANDLE hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
		if (NULL == hSCM)
			return false;

		TCHAR szPathName[MAX_PATH];
		GetModuleFileName (NULL, szPathName + 1, sizeof(szPathName) / sizeof(szPathName[0]));

		szPathName[0] = _T('\"');
		_tcscat_s(szPathName, _T("\" "));

		SC_HANDLE hService = CreateService(hSCM, SERVICE_NAME, SERVICE_INFO, 0, SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START, SERVICE_ERROR_NORMAL, szPathName, NULL, NULL, NULL, NULL, NULL);

		CloseServiceHandle(hSCM);
		CloseServiceHandle(hService);

		return (NULL != hService);
	}

	bool ProcessCommandLine(int argc, _TCHAR* argv[])
	{
		if (argc > 1)
		{
			if (0 == _tcsicmp(argv[1], _T("-install")))
			{
				if (!Install())
					MessageBox(NULL, _T("Service installation error"), SERVICE_INFO, NULL);
			}
			else if (0 == _tcsicmp(argv[1], _T("-uninstall")))
			{
				if (!Uninstall())
					MessageBox(NULL, _T("Service deinstallation error"), SERVICE_INFO, NULL);
			}
			else
				MessageBox(NULL, _T("Error in command line"), SERVICE_INFO, NULL);

			return true;
		}

		return false;
	}

	static HANDLE g_hStopEvent = NULL;

	void __stdcall CtrlHandler(DWORD dwControl)
	{
		switch (dwControl)
		{
		case SERVICE_CONTROL_STOP:
			SetEvent(g_hStopEvent);
			break;
		default:
			break;
		}
	}

	void __stdcall ServiceMain(DWORD, LPTSTR*)
	{
		SERVICE_STATUS_HANDLE hStatus = RegisterServiceCtrlHandler(SERVICE_NAME, CtrlHandler);
		if (hStatus == NULL)
			return;

		SERVICE_STATUS Status;
		Status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
		Status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_PARAMCHANGE | SERVICE_ACCEPT_NETBINDCHANGE;
		Status.dwWin32ExitCode = NOERROR;
		Status.dwServiceSpecificExitCode = 0;
		Status.dwCheckPoint = 0;
		Status.dwWaitHint = 0;

		Status.dwCurrentState = SERVICE_START_PENDING;
		SetServiceStatus(hStatus, &Status);

		// событие остановки сервера
		g_hStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		if (g_hStopEvent)
		{
			WSADATA Init;
			WSAStartup(MAKEWORD(2, 2), &Init);

			// чтение настроек из ини-файла
			TServerSettings settings;
			if (GetSettings(settings))
			{
				// запуск основной логики
				CTunnelServer server;
				if (server.Start(settings))
				{
					Status.dwCurrentState = SERVICE_RUNNING;
					SetServiceStatus(hStatus, &Status);

					WaitForSingleObject(g_hStopEvent, INFINITE);

					// остановка основной логики
					server.Stop();
				}
			}

			WSACleanup();
			CloseHandle(g_hStopEvent);
		}

		Status.dwCurrentState = SERVICE_STOPPED;
		SetServiceStatus(hStatus, &Status);
		return;
	}

	bool GetSettings(TServerSettings& settings)
	{
		TCHAR BaseFileName[MAX_PATH], FullFileName[MAX_PATH];
		DWORD dwFullFileNameLength = GetModuleFileName(NULL, FullFileName, MAX_PATH);
		if (0 == dwFullFileNameLength || MAX_PATH == dwFullFileNameLength)
			return false;

		DWORD dwBaseFileNameLength = GetModuleBaseName(GetCurrentProcess(), NULL, BaseFileName, MAX_PATH);
		if (0 == dwBaseFileNameLength || MAX_PATH == dwBaseFileNameLength)
			return false;

		FullFileName[dwFullFileNameLength - dwBaseFileNameLength] = _T('\0');
		_tcscat_s(FullFileName, SETTINGS_FILENAME);

		settings.Port = GetPrivateProfileInt(SETTINGS_SERVER, SETTINGS_SERVER_PORT, SERVER_DEFAULT_PORT, FullFileName);

		return true;
	}

	bool GetDBSettings(TDBSettings& settings)
	{
		char BaseFileName[MAX_PATH], FullFileName[MAX_PATH];

		DWORD dwFullFileNameLength = GetModuleFileNameA(NULL, FullFileName, MAX_PATH);
		if (0 == dwFullFileNameLength || MAX_PATH == dwFullFileNameLength)
			return false;

		DWORD dwBaseFileNameLength = GetModuleBaseNameA(GetCurrentProcess(), NULL, BaseFileName, MAX_PATH);
		if (0 == dwBaseFileNameLength || MAX_PATH == dwBaseFileNameLength)
			return false;

		FullFileName[dwFullFileNameLength - dwBaseFileNameLength] = _T('\0');
		strcat_s(FullFileName, SETTINGS_DB_FILENAME);

		GetPrivateProfileStringA(SETTINGS_DB, SETTINGS_DB_USER, DB_DEFAULT_USER, settings.User, sizeof(settings.User), FullFileName);
		GetPrivateProfileStringA(SETTINGS_DB, SETTINGS_DB_PASS, DB_DEFAULT_PASS, settings.Pass, sizeof(settings.Pass), FullFileName);
		GetPrivateProfileStringA(SETTINGS_DB, SETTINGS_DB_DATA, DB_DEFAULT_DATA, settings.Data, sizeof(settings.Data), FullFileName);
		GetPrivateProfileStringA(SETTINGS_DB, SETTINGS_DB_SERV, DB_DEFAULT_SERV, settings.Serv, sizeof(settings.Serv), FullFileName);
		settings.Port = GetPrivateProfileIntA(SETTINGS_DB, SETTINGS_DB_PORT, DB_DEFAULT_PORT, FullFileName);

		return true;
	}
}

