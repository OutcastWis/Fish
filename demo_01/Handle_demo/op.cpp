#include "pch.h"

#include "op.h"
#include "stl_import.h"

HANDLE g_hBoundry = NULL, g_hNamespace = NULL, g_hSingleton = NULL;
BOOL g_bNamespaceOpened = FALSE; // whether or not the namespace was created or open for clean-up

namespace op
{
    CString GetErrorMsg()
    {
        CString ret;
        DWORD err = GetLastError();

        // buffer that get the error message
        HLOCAL hLocal = NULL;
        // use the default system locale
        DWORD systemLocale = MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL);

        BOOL ok = FormatMessage(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_ALLOCATE_BUFFER,
            NULL, err, systemLocale, (PTSTR)&hLocal, 0, NULL
        );

        if (!ok)
        {
            // 可能是和网络相关的错误
            HMODULE hDll = LoadLibraryEx(_T("netmsg.dll"), NULL, DONT_RESOLVE_DLL_REFERENCES);
            if (hDll != NULL)
            {
                ok = FormatMessage(
                    FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_ALLOCATE_BUFFER,
                    hDll, err, systemLocale, (PTSTR)&hLocal, 0, NULL
                );
                FreeLibrary(hDll);
            }
        }

        ret.SetString((PCTSTR)LocalLock(hLocal));
        LocalFree(hLocal);

        return ret;
    }

    CString InheritHANDLE()
    {
        // 创建一个可继承的句柄, 父进程必须分配并初始化一个SECURITY_ATTRIBUTES结构
        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(sa);
        sa.lpSecurityDescriptor = NULL;
        sa.bInheritHandle = TRUE;   // 可继承
        // 创建了一个互斥量, 并获得了其可继承的句柄
        HANDLE hMutex = CreateMutex(&sa, FALSE, NULL); 

        // 
        if (hMutex == NULL)
        {
            return GetErrorMsg();
        }


        //
        if (hMutex)
            CloseHandle(hMutex);

        return _T("");
    }

    CString ModifyHandleAttr()
    {
        HANDLE hObj = CreateMutex(NULL, FALSE, NULL);
        if (hObj == NULL)
            return GetErrorMsg();

        // 打开句柄的继承属性
        SetHandleInformation(hObj, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
        // 关闭继承属性
        SetHandleInformation(hObj, HANDLE_FLAG_INHERIT, 0);
        
        // 获取句柄属性
        DWORD dwFlags;
        GetHandleInformation(hObj, &dwFlags);
        // 判断句柄是否可继承
        BOOL fHandleInInheritable = (0 != (dwFlags & HANDLE_FLAG_INHERIT));

        // 告诉系统不允许关闭句柄
        SetHandleInformation(hObj, HANDLE_FLAG_PROTECT_FROM_CLOSE, HANDLE_FLAG_PROTECT_FROM_CLOSE);
        try {
            CloseHandle(hObj);  // 调试器下运行, 会引发异常. 否则只会返回FALSE
        }
        catch (...) {}

        SetHandleInformation(hObj, HANDLE_FLAG_PROTECT_FROM_CLOSE, 0);
        CloseHandle(hObj);

        return _T("");
    }

    CString NamedObject()
    {
        // 很多创建内核对象的函数, 都含有一个变量指向名称. 名称传入NULL, 表示创建匿名对象.
        // 内核对象的名称是全局的, 且没有机制提示你是否名称重复.

        HANDLE hMutex = CreateMutex(NULL, FALSE, _T("John"));
        HANDLE hSem = CreateSemaphore(NULL, 1, 1, _T("John")); // 由于重名, 且类型不一致, 导致创建失败
        ASSERT(hSem == NULL);
        DWORD dwErrorCode = GetLastError(); // ERROR_INVALID_HANDLE
        ASSERT(dwErrorCode == ERROR_INVALID_HANDLE);


        // 假设这是另一个进程中去获取命名对象. 由于名称已存在, 且类型也都是互斥量, 所以
        // 会返回先前构造的
        {
            HANDLE hMutex2 = CreateMutex(NULL, FALSE, _T("John"));
            ASSERT(GetLastError() == ERROR_ALREADY_EXISTS);
            if (hMutex2) CloseHandle(hMutex2);
        }


        if (hMutex) CloseHandle(hMutex);

        return _T("");
    }

    CString PrivateNamespace()
    {
        // create boundry descriptor
        g_hBoundry = CreateBoundaryDescriptor(_T("wzj-Boundry"), 0);
        // create a SID corresponding to the Local Administrator group
        BYTE localAdminSID[SECURITY_MAX_SID_SIZE];
        PSID pLocalAdminSID = &localAdminSID;
        DWORD cbSID = sizeof(localAdminSID);
        if (!CreateWellKnownSid(WinBuiltinAdministratorsSid, NULL, pLocalAdminSID, &cbSID))
        {
            return GetErrorMsg();
        }

        // Associate the Local Admin SID to boundary descriptor.
        // Only app running under an administrator user will be able to access the 
        // kernel objects in the same namespace
        if (!AddSIDToBoundaryDescriptor(&g_hBoundry, pLocalAdminSID))
            return GetErrorMsg();

        // Create the namespace for Local Admin only
        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = FALSE;
        if (!ConvertStringSecurityDescriptorToSecurityDescriptor(
            _T("D:(A;;GA;;;BA)"), SDDL_REVISION_1, &sa.lpSecurityDescriptor, NULL
        ))
            return GetErrorMsg();
        g_hNamespace = CreatePrivateNamespace(&sa, g_hBoundry, _T("wzj-Boundry")); // return NULL, if already exists
        // release
        LocalFree(sa.lpSecurityDescriptor);

        // Check the private namespace creation result
        DWORD dwLastError = GetLastError();
        if (g_hNamespace == NULL)
        {
            // Nothing to do if access is denied. 
            // This code must run under a Local Admin
            if (dwLastError == ERROR_ACCESS_DENIED)
            {
                return _T("You must running as Administrator\n");
            }

            if (dwLastError == ERROR_ALREADY_EXISTS)
            {
                // If another instance has already created the namespace, we need to
                // open it instead
                g_hNamespace = OpenPrivateNamespace(g_hBoundry, _T("wzj-Boundry"));
                if (g_hNamespace == NULL)
                    return GetErrorMsg();
                g_bNamespaceOpened = TRUE;
            }
            else
                return GetErrorMsg();
        }


        // Example to use the private namespace: create a mutex object with a 
        // name based on the private namespace
        {
            TCHAR szMutexName[64];
            StringCchPrintf(szMutexName, _countof(szMutexName), _T("%s\\%s"), _T("wzj-Boundry"), _T("Singleton"));

            g_hSingleton = CreateMutex(NULL, FALSE, szMutexName);
            if (GetLastError() == ERROR_ALREADY_EXISTS)
            {
                return _T("Another instance of Singleton is running");
            }
        }

        return _T("");
    }

    void ClearPrivateNamespace()
    {
        if (g_hSingleton != NULL)
            CloseHandle(g_hSingleton);

        if (g_hNamespace != NULL)
        {
            if (g_bNamespaceOpened) // opened namespace
                ClosePrivateNamespace(g_hNamespace, 0);
            else // created namespace
                ClosePrivateNamespace(g_hNamespace, PRIVATE_NAMESPACE_FLAG_DESTROY);
        }
        
        if (g_hBoundry != NULL)
            DeleteBoundaryDescriptor(g_hBoundry);
    }

    void CopyHandle()
    {
        // 情况1: 需要3个进程参与, 源进程S, 目标进程T, 中间进程C.
        {
            // 源进程1将句柄2, 借由进程C调用DuplicateHandle, 复制给目标进程3. hObj接受句柄在目标进程中的值.
            // TRUE表示可继承, DUPLICATE_SAME_ACCESS表明目标句柄和源句柄具有相同访问掩码
            HANDLE hObj = NULL;
            DuplicateHandle((HANDLE)1, (HANDLE)2, (HANDLE)3, &hObj, 0, TRUE, DUPLICATE_SAME_ACCESS);
        }

        // 情况2: 涉及2个程序. 源进程S将句柄复制给目标进程T
        {
            // 创建一个源进程下的对象句柄
            HANDLE hObjInProcessS = CreateMutex(NULL, FALSE, NULL);
            // 获取pid为1的进程的进程句柄, 作为目标进程
            HANDLE hProcessT = OpenProcess(PROCESS_ALL_ACCESS, FALSE, 1);

            // 复制. 使得目标进程T也可访问当下进程的互斥量句柄
            HANDLE hObjInProcessT = NULL;
            DuplicateHandle(GetCurrentProcess(), hObjInProcessS, hProcessT, &hObjInProcessT, 0,
                FALSE, DUPLICATE_SAME_ACCESS);

            // ... do something

            // no longer need to commuicate with Process T
            CloseHandle(hProcessT);

            //
            CloseHandle(hObjInProcessS);
        }

        // 情况3: 同一进程内, 获得另一个不同权限的句柄
        {
            // 创建一个读写权限的句柄
            HANDLE hFileMapRW = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 10240, NULL);

            // 创建另一个只有读权限的句柄
            HANDLE hFileMapRO = NULL;
            DuplicateHandle(GetCurrentProcess(), hFileMapRW, GetCurrentProcess(), &hFileMapRO,
                FILE_MAP_READ, FALSE, 0);

            // ... do something

            // 
            CloseHandle(hFileMapRO);
            CloseHandle(hFileMapRW);
        }
    }
}