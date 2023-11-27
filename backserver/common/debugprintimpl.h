#pragma once

#ifdef OUTPUT_DEBUG

inline LPCSTR basefilename(LPCSTR fname)
{
	LPCSTR ptr = strrchr(fname, '\\');
	return (ptr) ? (ptr + 1) : fname;
}

inline void debugprint(LPCSTR Format, ...)
{
	va_list arglist;
	va_start(arglist, Format);

	bool bOk = false;
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
				bOk = true;
			}
			HeapFree(GetProcessHeap(), 0, pBuffer);
		}
	} while (!bOk);

	va_end(arglist);
}

#endif OUTPUT_DEBUG

#ifdef OUTPUT_DEBUG
#define dbg(x, ...) debugprint("%s:%s:%d %s (tid: %d) " x "\n", PluginName, basefilename(__FILE__), __LINE__, __FUNCTION__, GetCurrentThreadId(), __VA_ARGS__)
#else OUTPUT_DEBUG
#define dbg(x, ...)
#endif OUTPUT_DEBUG
