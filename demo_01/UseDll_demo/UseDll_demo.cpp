#include "pch.h"

#include <strsafe.h>
#include <delayimp.h>
#include <stdlib.h>

#include <cassert>
#include "../Dll_demo/op.h"

// 如果DLL的名字中有后缀.dll, 则会先去KnownsDLLs中寻找, 然后在按搜索规则. 否则直接按搜索规则
// KnownsDLLs在注册表HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\Session Manager\KnownDLLs

// dll搜索规则
// 1. 包含可执行文件的目录
// 2. Windows的系统目录, 可以用过GetSystemDirectory得到
// 3. 16位系统的子目录, 即Windows目录中的System子目录
// 4. Windows目录, 该目录可以通过GetWindowsDirectory得到
// 5. 进程当前目录
// 6. PATH环境中所列出的目录



// The name of the Delay-Load module (only used by this sample app)
TCHAR g_szDelayLoadModuleName[] = TEXT("dll_demo");


// Forward function prototype
LONG WINAPI DelayLoadDllExceptionFilter(PEXCEPTION_POINTERS pep);

void IsModuleLoaded(PCTSTR pszModuleName) {

	HMODULE hmod = GetModuleHandle(pszModuleName);
	TCHAR sz[100];
	
	_stprintf_s(sz, _T("Moudle \"%s\" is %s loaded"), pszModuleName, (hmod == NULL) ? _T("not ") : _T(""));

	TCHAR szTitle[MAX_PATH] = {};
	GetModuleFileName(NULL, szTitle, MAX_PATH - 1);
	MessageBox(GetActiveWindow(), sz, szTitle, MB_OK);
}

DWORD WINAPI ThreadProc(PVOID)
{
	// do nothing
	return 0;
}

// 延迟加载dll. 需要在配置--链接器中指定要延迟加载的dll
// Statically link __delayLoadHelper2/__FUnloadDelayLoadedDLL2
#pragma comment(lib, "Delayimp.lib")
int main()
{
	__try
	{
		IsModuleLoaded(g_szDelayLoadModuleName);

		int x = myfunc::add(1, 2);
		assert(x == 3);

		IsModuleLoaded(g_szDelayLoadModuleName);

		x = multi(4, 5);
		assert(x == 20);

		HANDLE hThread = CreateThread(NULL, 0, ThreadProc, NULL, 0, NULL);
		Sleep(1000);
		CloseHandle(hThread);

		PCSTR pszDll = "dll_demo.dll";
		__FUnloadDelayLoadedDLL2(pszDll);

		// Use Debug.Modules to see that the DLL is now unloaded
		IsModuleLoaded(g_szDelayLoadModuleName);

		x = multi(8,9);  // Attempt to call delay-load function
		assert(x == 72);

		// Use Debug.Modules to see that the DLL is loaded again
		IsModuleLoaded(g_szDelayLoadModuleName);
	}
	__except (DelayLoadDllExceptionFilter(GetExceptionInformation()))
	{

	}

    return 0;
}


LONG WINAPI DelayLoadDllExceptionFilter(PEXCEPTION_POINTERS pep) {

	// Assume we recognize this exception
	LONG lDisposition = EXCEPTION_EXECUTE_HANDLER;

	// If this is a Delay-load problem, ExceptionInformation[0] points 
	// to a DelayLoadInfo structure that has detailed error info
	PDelayLoadInfo pdli =
		PDelayLoadInfo(pep->ExceptionRecord->ExceptionInformation[0]);

	// Create a buffer where we construct error messages
	char sz[500] = { 0 };

	switch (pep->ExceptionRecord->ExceptionCode) {
	case VcppException(ERROR_SEVERITY_ERROR, ERROR_MOD_NOT_FOUND):
		// The DLL module was not found at runtime
		StringCchPrintfA(sz, _countof(sz), "Dll not found: %s", pdli->szDll);
		break;

	case VcppException(ERROR_SEVERITY_ERROR, ERROR_PROC_NOT_FOUND):
		// The DLL module was found, but it doesn't contain the function
		if (pdli->dlp.fImportByName) {
			StringCchPrintfA(sz, _countof(sz), "Function %s was not found in %s",
				pdli->dlp.szProcName, pdli->szDll);
		}
		else {
			StringCchPrintfA(sz, _countof(sz), "Function ordinal %d was not found in %s",
				pdli->dlp.dwOrdinal, pdli->szDll);
		}
		break;

	default:
		// We don't recognize this exception
		lDisposition = EXCEPTION_CONTINUE_SEARCH;
		break;
	}

	if (lDisposition == EXCEPTION_EXECUTE_HANDLER) {
		// We recognized this error and constructed a message, show it
		char szTitle[MAX_PATH] = {};
		GetModuleFileNameA(NULL, szTitle, MAX_PATH - 1);
		MessageBoxA(GetActiveWindow(), sz, szTitle, MB_OK);
	}

	return(lDisposition);
}