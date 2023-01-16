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
    op::PrintAllEnvVariable(envp);

    // 返回进程的提升类型和是否正在以管理员运行
    {
        TOKEN_ELEVATION_TYPE token;
        BOOL admin;
        op::GetProcessElevation(&token, &admin);
    }

    return 0;
}