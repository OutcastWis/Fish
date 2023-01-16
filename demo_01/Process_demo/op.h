#pragma once

namespace op
{
    // 获取base address
    void DumpModule();

    // 打印所有环境变量
    void PrintAllEnvVariable(const TCHAR* envp[]);

    // 打印key对应的环境变量
    void PrintEnvVariable(const CString& key);

    // 返回进程的提升类型和是否正在以管理员运行
    // @param pElevationType [out], 提升类型
    // @param pIsAdmin [out], 是否以管理员运行
    BOOL GetProcessElevation(TOKEN_ELEVATION_TYPE* pElevationType, BOOL* pIsAdmin);
}