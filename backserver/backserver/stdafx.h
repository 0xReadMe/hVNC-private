#pragma once

#define _CRT_SECURE_NO_WARNINGS

#include <sdkddkver.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <WinSock2.h>
#include <Shlwapi.h>
#include <Objbase.h>

#include <tchar.h>
#include <time.h>
#include <stdint.h>
#include <unordered_map>
#include <unordered_set>
#include <string>

#define OUTPUT_DEBUG
#define PluginName "backserver"
#include <debugprintimpl.h>

// ���������� ��� ������ � �������� ������� ��
#define WINDOWS_SERVICE
// TODO: ������ � �������� ����������� ����������
// TODO: ����c������������� ������