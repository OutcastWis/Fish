#include "stdafx.h"
#include "op.h"

int _tmain(int argc, TCHAR* argv[], TCHAR* envp[])
{
    op::DumpModule();
    
    // 分解命令行参数
    {
        int nNumArgs;
        PWSTR* ppArgv = CommandLineToArgvW(GetCommandLine(), &nNumArgs);

        // do something

        // free
        HeapFree(GetProcessHeap(), 0, ppArgv);
    }
    // 环境参数
    {
        // 此方法不会有=开头的字串
        if (false){
            TCHAR** i = envp;
            while (i)
            {
                if (*i == NULL)
                    break;
                _tprintf(_T("%s\r\n"), *i);
                i++;
            }
        }
        // 此方法全部
        {
            PTSTR pEnvBlock = GetEnvironmentStrings();
            PTSTR i = pEnvBlock;
            while (i)
            {
                 _tprintf(_T("%s\r\n"), i);
                 while (*i != _T('\0')) ++i;
                 ++i;
                 if (*i == 0)
                     break;
            }
        }
    }
    return 0;
}