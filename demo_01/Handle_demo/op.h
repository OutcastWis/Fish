#pragma once

// ���̼������
namespace op
{
    // ��ȡGetLastError��Ӧ�Ĵ�����Ϣ
    CString GetErrorMsg();

    // 1. ʹ�þ���̳�
    CString InheritHANDLE();

    // �޸ľ������
    CString ModifyHandleAttr();

    // 2. ʹ����������
    CString NamedObject();

    // ʹ��ר�������ռ�, ������������
    // ʹ������: 
    // auto i = op::PrivateNamespace();
    // if (!i.IsEmpty()) // �޴�
    //     _tprintf(_T("%s"), static_cast<const TCHAR*>(i));
    // else
    // {
    //     while (true) { // �ȴ��˳�
    //         TCHAR cmd[256] = {};
    //         _tscanf_s(_T("%s"), cmd);
    //         if (_tcsnicmp(cmd, _T("exit"), 5) == 0)
    //             break;
    //     }
    // }
    //
    // op::ClearPrivateNamespace(); // ����ر�
    CString PrivateNamespace();

    // ����ר�������ռ�, ���PrivateNamespaceʹ��
    void ClearPrivateNamespace();

    // 3. ���ƶ�����
    void CopyHandle();
}