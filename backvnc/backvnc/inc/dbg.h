//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// VNC project. Version 1.9.17.3
//	
// module: dbg.h
// $Revision: 187 $
// $Date: 2014-07-11 16:48:53 +0400 (Пт, 11 июл 2014) $
// description:
//	Debug-buld support routines: DbgPrint(), ASSERT() and checked pool allocations.

#pragma once

#include <crtdbg.h>

#define BAD_PTR		(LONG_PTR)0xBAADF00D
#define	PAGE_SIZE	0x1000

extern	HANDLE	g_AppHeap;

#define Alloc(x)		LocalAlloc(LMEM_FIXED, x)
#define Free(x)			LocalFree(x)
#define Realloc(x,y)	LocalReAlloc(x,y, LMEM_FIXED)

#define hAlloc(x)		HeapAlloc(g_AppHeap, 0, x)
#define hFree(x)		HeapFree(g_AppHeap, 0, x)
#define hRealloc(x,y)	HeapReAlloc(g_AppHeap, 0, x, y)

#define vAlloc(x)		VirtualAlloc(0, x, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)
#define vFree(x)		VirtualFree(x, 0, MEM_RELEASE)

__inline void vProtect(PVOID Ptr, ULONG Size)
{
	DWORD OldProtect = 0;

	VirtualProtect(Ptr, Size, PAGE_EXECUTE_READWRITE, &OldProtect);
}

#ifdef _DBG

extern LPTSTR g_CurrentProcessName;
extern HANDLE g_DebugFileSync;

__inline void debugtofile(LPCSTR string)
{
	WaitForSingleObject(g_DebugFileSync, INFINITE);

	WCHAR TempPath[MAX_PATH], FileName[MAX_PATH];
	GetTempPathW(MAX_PATH, TempPath);
	GetTempFileNameW(TempPath, L"VNC", 0x1234, FileName);

	HANDLE hFile = CreateFileW(FileName, GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_ALWAYS, 0, NULL);
	if (hFile != INVALID_HANDLE_VALUE)
	{
		SetFilePointer(hFile, 0, NULL, FILE_END);

		DWORD dwSize = lstrlenA(string);
		WriteFile(hFile, string, dwSize, &dwSize, NULL);
		FlushFileBuffers(hFile);

		CloseHandle(hFile);
	}

	ReleaseMutex(g_DebugFileSync);
}

__inline void debugprint(LPCSTR Format, ...)
{
	va_list arglist;
	va_start(arglist, Format);

	BOOL bOk = FALSE;
	LPSTR pBuffer = NULL;
	DWORD dwBufferSize = lstrlenA(Format);
	
	do
	{
		dwBufferSize += 0x100;
		pBuffer = (LPSTR)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwBufferSize);
		if (pBuffer)
		{
			if (wvnsprintfA(pBuffer, dwBufferSize, Format, arglist) >= 0)
			{
				OutputDebugStringA(pBuffer);

				// записать в файл
				if (g_DebugFileSync)
					debugtofile(pBuffer);

				bOk = TRUE;
			}
			HeapFree(GetProcessHeap(), 0, pBuffer);
		}
	} while (!bOk);

	va_end(arglist);
}

#define DbgPrint(fmt, ...) debugprint("[%s:%u:%u] VNCDLL (%s:%u) " fmt "\n", g_CurrentProcessName ? g_CurrentProcessName : "", GetCurrentProcessId(), GetCurrentThreadId(), __FUNCTION__, __LINE__, __VA_ARGS__)
#define ASSERT(x) _ASSERT(x)

#else

#define DbgPrint(fmt, ...) 
#define ASSERT(x)

#endif

