#pragma once

#ifndef WINVER
#define WINVER 0x0501
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif						

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Sddl.h>

#include <tchar.h>
#include <psapi.h>
#include <wininet.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <winsock2.h>

//#define _DBG TRUE

#undef strlen
#undef _stricmp
#undef _mbsicmp
#undef strscmp
#undef wcscmp
#undef wsclen
#undef malloc
#undef free
#undef strupr
#undef wcsupr
#undef _mbsupr
#undef strtoul
#undef strcat
#undef wcscat

#undef _tcsrchr
#undef strchr
#undef wcschr
#undef strrchr
#undef wcsrchr
#undef _tcsicmp

#define strlen		lstrlenA
#define wcslen		lstrlenW
#define strcmp		lstrcmp
#define wcscmp		lstrcmpW
#define _stricmp	lstrcmpiA
#define _tcsicmp	lstrcmpiA
#define	_mbsicmp	lstrcmpiA
#define strcat		lstrcatA
#define wcscat		lstrcatW

#define malloc(x)		LocalAlloc(LPTR, x)
#define free(x)			LocalFree(x)
#define	realloc(x, y)	LocalReAlloc(x, y, LMEM_MOVEABLE)

#define strupr	_strupr
#define wcsupr	_wcsupr
#define	_mbsupr	_strupr

#define strtoul(a,b,c)	StrToIntA(a)
#define wcstoul(a,b,c)	StrToIntW(a)
#define strchr			StrChrA
#define wcschr			StrChrW
#define strrchr(a,b)	StrRChrA(a, NULL, b)
#define wcsrchr(a,b)	StrRChrW(a, NULL, b)
#define _tcsrchr(a,b)	StrRChr(a, NULL, b)

#pragma warning(push)
#pragma warning(disable:4005) // macro redefinition
#include "ntdll.h"
#include <ntstatus.h>
#pragma warning(pop)

#pragma warning (disable:4996)	// 'wcscpy': This function or variable may be unsafe. Consider using wcscpy_s instead.

#include "dbg.h"
#include "listsup.h"

typedef INT	WINERROR;
#define ERROR_UNSUCCESSFULL	0xffffffff
#define	INVALID_INDEX		(-1)

#define MAX_PATH_BYTES (MAX_PATH*sizeof(_TCHAR))

#define cstrlenW(str)	(sizeof(str)/sizeof(WCHAR))-1
#define cstrlenA(str)	(sizeof(str)-1)

// constant string length
#if _UNICODE
	C_ASSERT(FALSE);
	#define cstrlen(str)	cstrlenW(str)
#else
	#define cstrlen(str)	cstrlenA(str)
#endif

// minimum buffer size
#define BUFFER_INCREMENT	 0x1000

// timer period macros
#define _RELATIVE(x)		-(x)
#define _SECONDS(x)			(LONGLONG)x*10000000
#define _MILLISECONDS(x)	(LONGLONG)x*10000
#define _MINUTES(x)			(LONGLONG)x*600000000

#define	htonS(x)			((LOBYTE(x) << 8) + HIBYTE(x))
#define	htonL(x)			((LOBYTE(LOWORD(x)) << 24) + (HIBYTE(LOWORD(x)) << 16) + (LOBYTE(HIWORD(x)) << 8) + HIBYTE(HIWORD(x)))

PVOID __stdcall	AppAlloc(ULONG Size);
VOID __stdcall	AppFree(PVOID pMem);
