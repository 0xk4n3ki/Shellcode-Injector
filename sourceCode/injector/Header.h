#include <iostream>
#include <Windows.h>

// Define CreateThread function prototype
using customCreateThread = HANDLE(NTAPI*)(
	LPSECURITY_ATTRIBUTES   lpThreadAttributes,
	SIZE_T                  dwStackSize,
	LPTHREAD_START_ROUTINE  lpStartAddress,
	__drv_aliasesMem LPVOID lpParameter,
	DWORD                   dwCreationFlags,
	LPDWORD                 lpThreadId
	);

using customConvertThreadToFiber = LPVOID(NTAPI*)(
	LPVOID lpParameter
);

using customVirtualAlloc = LPVOID(NTAPI*)(
	LPVOID lpAddress,
	SIZE_T dwSize,
	DWORD  flAllocationType,
	DWORD  flProtect
);

using customHeapAlloc = LPVOID(NTAPI*)(
	HANDLE hHeap,
	DWORD  dwFlags,
	SIZE_T dwBytes
);

using customVirtualProtect = BOOL(NTAPI*)(
	LPVOID lpAddress,
	SIZE_T dwSize,
	DWORD  flNewProtect,
	PDWORD lpflOldProtect
);

using customCreateFiber = LPVOID(NTAPI*)(
	SIZE_T                dwStackSize,
	LPFIBER_START_ROUTINE lpStartAddress,
	LPVOID                lpParameter
);

using customSwitchToFiber = void(NTAPI*)(
	LPVOID lpFiber
);

using customEnumProcesses = BOOL(NTAPI*)(
	DWORD* lpidProcess,
	DWORD   cb,
	LPDWORD lpcbNeeded
);

using customOpenProcess = HANDLE(NTAPI*)(
	DWORD dwDesiredAccess,
	BOOL  bInheritHandle,
	DWORD dwProcessId
);

using customEnumProcessModules = BOOL(NTAPI*)(
	HANDLE  hProcess,
	HMODULE* lphModule,
	DWORD   cb,
	LPDWORD lpcbNeeded
);

using customGetModuleBaseNameW = DWORD(NTAPI*)(
	HANDLE  hProcess,
	HMODULE hModule,
	LPWSTR  lpBaseName,
	DWORD   nSize
);

using customCloseHandle = BOOL(NTAPI*)(
	HANDLE hObject
);

using customVirtualAllocEx = LPVOID(NTAPI*)(
	HANDLE hProcess,
	LPVOID lpAddress,
	SIZE_T dwSize,
	DWORD  flAllocationType,
	DWORD  flProtect
);

using customWriteProcessMemory = BOOL(NTAPI*)(
	HANDLE  hProcess,
	LPVOID  lpBaseAddress,
	LPCVOID lpBuffer,
	SIZE_T  nSize,
	SIZE_T* lpNumberOfBytesWritten
);

using customCreateRemoteThread = HANDLE(NTAPI*)(
	HANDLE                 hProcess,
	LPSECURITY_ATTRIBUTES  lpThreadAttributes,
	SIZE_T                 dwStackSize,
	LPTHREAD_START_ROUTINE lpStartAddress,
	LPVOID                 lpParameter,
	DWORD                  dwCreationFlags,
	LPDWORD                lpThreadId
);

using customVirtualProtectEx = BOOL(NTAPI*)(
	HANDLE hProcess,
	LPVOID lpAddress,
	SIZE_T dwSize,
	DWORD  flNewProtect,
	PDWORD lpflOldProtect
);

using customWaitForSingleObjectEx = DWORD(NTAPI*)(
	HANDLE hHandle,
	DWORD  dwMilliseconds,
	BOOL   bAlertable
);