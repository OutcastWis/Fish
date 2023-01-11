#pragma once

// 进程间对象共享
namespace op
{
    // 获取GetLastError对应的错误信息
    CString GetErrorMsg();

    // 1. 使用句柄继承
    CString InheritHANDLE();

    // 修改句柄属性
    CString ModifyHandleAttr();

    // 2. 使用命名对象
    CString NamedObject();

    // 使用专有命名空间, 保护命名对象
    // 使用例子: 
    // auto i = op::PrivateNamespace();
    // if (!i.IsEmpty()) // 无错
    //     _tprintf(_T("%s"), static_cast<const TCHAR*>(i));
    // else
    // {
    //     while (true) { // 等待退出
    //         TCHAR cmd[256] = {};
    //         _tscanf_s(_T("%s"), cmd);
    //         if (_tcsnicmp(cmd, _T("exit"), 5) == 0)
    //             break;
    //     }
    // }
    //
    // op::ClearPrivateNamespace(); // 清理关闭
    CString PrivateNamespace();

    // 清理专有命名空间, 配合PrivateNamespace使用
    void ClearPrivateNamespace();

    // 3. 复制对象句柄
    void CopyHandle();
}