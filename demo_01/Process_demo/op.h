#pragma once

namespace op
{
    // ��ȡbase address
    void DumpModule();

    // ��ӡ���л�������
    void PrintAllEnvVariable(const TCHAR* envp[]);

    // ��ӡkey��Ӧ�Ļ�������
    void PrintEnvVariable(const CString& key);

    // ���ؽ��̵��������ͺ��Ƿ������Թ���Ա����
    // @param pElevationType [out], ��������
    // @param pIsAdmin [out], �Ƿ��Թ���Ա����
    BOOL GetProcessElevation(TOKEN_ELEVATION_TYPE* pElevationType, BOOL* pIsAdmin);
}