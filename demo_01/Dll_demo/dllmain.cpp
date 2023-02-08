// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "pch.h"


// 约束很大, 只能进行非常简单的操作, 例如:
//   1. 不要调用WaitForSingleObject, 会死锁
//   2. LoadLibrary可能会循环依赖
BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved // 显示载入为0, 否则不为0
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        // The DLL is being mapped into the process' address space
        _tprintf(_T("DLL_PROCESS_ATTACH\n"));
        break;
    case DLL_THREAD_ATTACH:
        // A thread is being created
        _tprintf(_T("DLL_THREAD_ATTACH\n"));
        break;
    case DLL_THREAD_DETACH:
        _tprintf(_T("DLL_THREAD_DETACH\n"));
        // A thread is exiting cleanly
        break;
    case DLL_PROCESS_DETACH:
        _tprintf(_T("DLL_PROCESS_DETACH\n"));
        // The DLL is being unmapped from the process' address space
        break;
    }
    return TRUE; // Used only for DLL_PROCESS_ATTACH
}

