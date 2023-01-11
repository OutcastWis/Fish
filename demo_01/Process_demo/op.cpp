#include "stdafx.h"
#include "op.h"


extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace op
{
    void DumpModule()
    {
        // Get the base address of the running application
        // Can be different from the running module if this code is in a DLL
        HMODULE hModule = GetModuleHandle(NULL); // 得到的是可执行文件的基地址, 而不是dll的
        _tprintf(_T("with GetModuleHandle(NULL) - 0x%x\r\n"), hModule);

        // Use the pesudo-variable __ImageBase to get the address of the current module
        // hModule/hInstance
        _tprintf(_T("whit __ImageBase = 0x%x\r\n"), (HINSTANCE)&__ImageBase);

        // Use GetModuleHandleEx to get the address of the current module hModule/hInstance
        hModule = NULL;
        GetModuleHandleEx(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (PCTSTR)DumpModule, &hModule
        );
        _tprintf(_T("with GetModuleHandleEx = 0x%x\r\n"), hModule);
    }

    void PrintEnvVariable(const CString& key)
    {
        PTSTR pszValue = NULL;
        DWORD dwResult = GetEnvironmentVariable(key, pszValue, 0);
        if (dwResult != 0)
        {
            pszValue = (PTSTR)malloc(dwResult * sizeof(TCHAR));
            GetEnvironmentVariable(key, pszValue, dwResult * sizeof(TCHAR));
            _tprintf(_T("{\n\tkey: %s\n\tvalue: %s"), key, pszValue);
            free(pszValue);
        }
        else
            _tprintf(_T("No such key: %s"), key);
    }
}