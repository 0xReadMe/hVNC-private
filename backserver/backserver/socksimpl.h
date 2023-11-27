#pragma once

#include "rc4impl.h"

// 5 минут для keep-alive
static const unsigned int BackclientTimeout = 5 * 60;

struct TTunnel
{
	typedef std::unordered_map<TTunnelId, TTunnel*> TList, *PList;
	TTunnelId id;
	unsigned int maxusers;

	SOCKET control;
	unsigned int unique;

	SOCKET user;
	SOCKET back;
	unsigned int uport;
	unsigned int bport;

	PList tunnels;
	PCRITICAL_SECTION sync;
	backdb* db;
	acldb* access;
};

struct TConnection 
{
	SOCKET user;
	SOCKET sock;
	time_t time;
	rc4_key rc4toclient;
	rc4_key rc4fromclient;

	void Shutdown()
	{
		Socket::Shutdown(user);
		Socket::Shutdown(sock);
		time = 0;
	}
};

struct TAcceptConnection
{
    SOCKET sock;
    time_t time;

	void Shutdown()
	{
		Socket::Shutdown(sock);
		time = 0;
	}
};

// Получаем от системы заданный порт (или любой, если в параметре port 0)
bool GetPort(SOCKET& sock, unsigned int& port)
{
	bool bResult = false;

	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (SOCKET_ERROR != sock)
	{
		SOCKADDR_IN addr;
		ZeroMemory(&addr, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = port;
		addr.sin_addr.s_addr = INADDR_ANY;

		if (SOCKET_ERROR != bind(sock, (LPSOCKADDR)&addr, sizeof(addr)))
		{
			int len = sizeof(addr);

			// узнаем номер выделеного порта
			if (SOCKET_ERROR != getsockname(sock, (LPSOCKADDR)&addr, &len))
			{
				port = htons(addr.sin_port);
				bResult = (SOCKET_ERROR != listen(sock, 5));
			}
		}
	}

	if (!bResult)
	{
		closesocket(sock);
		sock = INVALID_SOCKET;
		port = 0;
	}

	return bResult;
}

template<class T>
bool connection_closed(const T& connection) 
{
	return (connection.time == 0);
}

template<class T>
void queue_clear_timeouted(T& queue) 
{
	time_t t = time(NULL);
	for (auto& a = queue.begin(); a != queue.end(); ++a)
	{
		if ((t - a->time) > Socket::TimeoutExtendedSec)
			a->Shutdown();
	}
	queue.remove_if(connection_closed<T::value_type>);
}

template<class T>
void queue_clear_all(T& queue) 
{
	for (auto& a = queue.begin(); a != queue.end(); ++a)
		a->Shutdown();
	queue.clear();
}

void AcceptConnection(std::list<TAcceptConnection> & uqueue, SOCKET & user, SOCKET & sock)
{
	queue_clear_timeouted(uqueue);

	if (Socket::IsValid(user) && !Socket::IsValid(sock))
	{
		if (uqueue.size() < 100)
		{
			TAcceptConnection c;
			c.sock = user;
			c.time = time(NULL);
			uqueue.push_back(c);
		}
		else
		{
			Socket::Shutdown(user);
			user = INVALID_SOCKET;
		}
	}
	else if (!Socket::IsValid(user) && Socket::IsValid(sock))
	{
		if (uqueue.size() > 0)
		{
			user = uqueue.front().sock;
			uqueue.pop_front();
		}
		else
		{
			Socket::Shutdown(sock);
			sock = INVALID_SOCKET;
		}
	}

	dbg("ok %08X %08X, queue size %d", user, sock, uqueue.size());
}

// Обслуживание установленного туннельного соединения для многопоточного клиента (SOCKS, vnc (1))
DWORD __stdcall TunnelHandlerThreadProc(LPVOID param)
{
	dbg("in");

	TTunnel* pTunnel = (TTunnel*)param;
	SOCKET control = pTunnel->control;
	SOCKET user = pTunnel->user;
	SOCKET back = pTunnel->back;

	if (Socket::IsValid(control) && Socket::IsValid(user) && Socket::IsValid(back))
	{
		std::list<TConnection> connections;
		std::list<TAcceptConnection> accepts;

		dbg("started, %08X", pTunnel->unique);

		fd_set fset;
		int sel = 0;

		do
		{
			timeval timeout;
			timeout.tv_sec = 2 * BackclientTimeout;
			timeout.tv_usec = 0;

			FD_ZERO(&fset);
			FD_SET(control, &fset);
            FD_SET(user, &fset);
            FD_SET(back, &fset);

			queue_clear_timeouted(connections);
			for (auto& c = connections.begin(); c != connections.end(); ++c)
			{
				FD_SET(c->user, &fset);
				FD_SET(c->sock, &fset);
			}

			sel = select(0, &fset, NULL, NULL, &timeout);
			if (sel > 0)
			{
				SOCKET newuser = INVALID_SOCKET, newback = INVALID_SOCKET;

				if (FD_ISSET(control, &fset))
				{
					dbg("control connection");

					CommonBackProto::TKeepAliveMessage msg;
					int len = sizeof(msg);
					if (len != recv(control, (char*)&msg, len, 0))
					{
						dbg("network FAIL");
						break;
					}
					if (CommonBackProto::BackKeepConnection != msg.status || pTunnel->unique != msg.unique)
					{
						dbg("protocol FAIL");
						break;
					}
				}

				if (FD_ISSET(user, &fset)) 
				{
					dbg("user-side connection");

					sockaddr_in addr;
					int len = sizeof(addr);
					newuser = accept(user, (LPSOCKADDR)&addr, &len);
					if (!Socket::IsValid(newuser))
					{
						dbg("network FAIL");
						break;
					}

					dbg("ip = %s", inet_ntoa(addr.sin_addr));

					unsigned int ip = addr.sin_addr.s_addr;
					if (!pTunnel->access->check(ip))
					{
						dbg("access denied!");

						Socket::Shutdown(newuser);
						newuser = INVALID_SOCKET;
					}
					else
					{
						if (connections.size() == pTunnel->maxusers)
						{
							dbg("no more users allowed for this service!");

							Socket::Shutdown(newuser);
							newuser = INVALID_SOCKET;
						}
						else
						{
							CommonBackProto::TKeepAliveMessage msg = { CommonBackProto::BackNewConnection, pTunnel->unique };
							if (sizeof(msg) != send(control, (char*)&msg, sizeof(msg), 0))
							{
								dbg("control FAIL");
								break;
							}
						}

					}
				}

				if (FD_ISSET(back, &fset)) 
				{
					dbg("backclient-side connection");
					newback = accept(back, NULL, NULL);
					if (!Socket::IsValid(newback))
					{
						dbg("network FAIL");
						break;
					}
				}

				if (Socket::IsValid(newuser) || Socket::IsValid(newback))
					AcceptConnection(accepts, newuser, newback);

				if (Socket::IsValid(newuser) && Socket::IsValid(newback))
				{
					dbg("new tunnel established");

					Socket::SetTimeout(newuser);
					Socket::SetTimeout(newback);

					TConnection с;
					с.sock = newback;
					с.user = newuser;
					с.time = time(NULL);
					prepare_rc4_key((unsigned char*)&pTunnel->unique, sizeof(pTunnel->unique), &с.rc4toclient);
					prepare_rc4_key((unsigned char*)&pTunnel->unique, sizeof(pTunnel->unique), &с.rc4fromclient);

					connections.push_back(с);
				}

				time_t t = time(NULL);
				for (auto& c = connections.begin(); c != connections.end(); ++c)
				{
					char buff[0x1000];

					if (FD_ISSET(c->user, &fset))
					{
						int recv_len = recv(c->user, buff, sizeof(buff), 0);
						if (recv_len > 0) 
						{
							rc4((unsigned char*)buff, recv_len, &c->rc4toclient);
							send(c->sock, buff, recv_len, 0);
							c->time = t;
						}
						else
							c->Shutdown();
					}

					if (FD_ISSET(c->sock, &fset)) 
					{
						int recv_len = recv(c->sock, buff, sizeof(buff), 0);
						if (recv_len > 0) 
						{
							rc4((unsigned char*)buff, recv_len, &c->rc4fromclient);
							send(c->user, buff, recv_len, 0);
							c->time = t;
						}
						else
							c->Shutdown();
					}
				}
				connections.remove_if(connection_closed<TConnection>);
			}
			else
			{
				for (auto& a = connections.begin(); a != connections.end(); ++a)
				{
					dbg("%08X, %08X", a->sock, a->user);
				}

				dbg("select fail %d, %d", sel, GetLastError());
			}
		}
		while (sel > 0);

		queue_clear_all(connections);
		queue_clear_all(accepts);
	}

	dbg("finish");

	// Освободить ресурсы и удалить себя из списка
	if (pTunnel->sync && pTunnel->tunnels)
	{
		EnterCriticalSection(pTunnel->sync);
		pTunnel->tunnels->erase(pTunnel->id);
		pTunnel->db->del(pTunnel->id);
		LeaveCriticalSection(pTunnel->sync);
	}

	Socket::Shutdown(user);
	Socket::Shutdown(back);
	Socket::Shutdown(control);

	delete param;

	return 0;
}

//// Обслуживание установленного туннельного соединения для однопоточного клиента
//DWORD __stdcall TunnelSingleHandlerThreadProc(LPVOID param)
//{
//	dbg("in");
//
//	TTunnel* pTunnel = (TTunnel*)param;
//
//	SOCKET newuser = pTunnel->user;
//	SOCKET back = pTunnel->control;
//	SOCKET user = INVALID_SOCKET;
//
//	bool ok = true;
//	int data_size = 0;
//
//	rc4_key rc4toclient;
//	rc4_key rc4fromclient;
//	prepare_rc4_key((unsigned char*)&pTunnel->unique, sizeof(pTunnel->unique), &rc4toclient);
//	prepare_rc4_key((unsigned char*)&pTunnel->unique, sizeof(pTunnel->unique), &rc4fromclient);
//
//	if (Socket::IsValid(newuser) && Socket::IsValid(back))
//	{
//		dbg("started, %08X", pTunnel->unique);
//
//		fd_set fset;
//		int sel = 0;
//
//		do
//		{
//			dbg("select cycle");
//			timeval timeout;
//			timeout.tv_sec = 2 * BackclientTimeout;
//			timeout.tv_usec = 0;
//
//			FD_ZERO(&fset);
//			FD_SET(newuser, &fset);
//			FD_SET(back, &fset);
//			if (Socket::IsValid(user))
//				FD_SET(user, &fset);
//
//			sel = select(0, &fset, NULL, NULL, &timeout);
//			if (sel > 0)
//			{
//				if (FD_ISSET(newuser, &fset))
//				{
//					dbg("user-side connection");
//
//					sockaddr_in addr;
//					int len = sizeof(addr);
//					SOCKET testuser = accept(newuser, (LPSOCKADDR)&addr, &len);
//					if (!Socket::IsValid(testuser))
//					{
//						dbg("network FAIL");
//						break;
//					}
//
//					if (Socket::IsValid(user))
//					{
//						dbg("client already online!");
//						Socket::Shutdown(testuser);
//						testuser = INVALID_SOCKET;
//					}
//
//					unsigned int ip = addr.sin_addr.s_addr;
//					if (!pTunnel->access->check(ip))
//					{
//						dbg("access denied!");
//
//						Socket::Shutdown(testuser);
//						testuser = INVALID_SOCKET;
//					}
//
//					if (Socket::IsValid(testuser))
//					{
//						user = testuser;
//						Socket::SetTimeout(user);
//						dbg("user connected!");
//					}
//
//					continue;
//				}
//
//				char buffer[0x1000];
//				if (FD_ISSET(user, &fset))
//				{
//					int recv_len = recv(user, buffer, sizeof(buffer), 0);
//					if (recv_len < 1)
//					{
//						ok = (0 == recv_len) ? true : false;
//						break;
//					}
//					rc4((unsigned char*)buffer, recv_len, &rc4toclient);
//					if (recv_len != send(back, buffer, recv_len, 0))
//					{
//						ok = false;
//						break;
//					}
//					data_size += recv_len;
//				}
//
//				if (FD_ISSET(back, &fset))
//				{
//					int recv_len = recv(back, buffer, sizeof(buffer), 0);
//					if (recv_len < 1)
//					{
//						ok = (0 == recv_len) ? true : false;
//						break;
//					}
//					rc4((unsigned char*)buffer, recv_len, &rc4fromclient);
//					if (recv_len != send(user, buffer, recv_len, 0))
//					{
//						ok = false;
//						break;
//					}
//					data_size += recv_len;
//				}
//			}
//			else
//			{
//				dbg("server closes the connection, %d", sel);
//			}
//		} while (sel > 0);
//
//		dbg("stop client data exchange, sent %d bytes, result %s", data_size, ok ? "OK" : "FAIL");
//	}
//
//	dbg("finish");
//
//	// Освободить ресурсы и удалить себя из списка
//	if (pTunnel->sync && pTunnel->tunnels)
//	{
//		EnterCriticalSection(pTunnel->sync);
//		pTunnel->tunnels->erase(pTunnel->id);
//		pTunnel->db->del(pTunnel->id);
//		LeaveCriticalSection(pTunnel->sync);
//	}
//
//	Socket::Shutdown(newuser);
//	Socket::Shutdown(back);
//	Socket::Shutdown(user);
//
//	delete param;
//
//	return 0;
//}