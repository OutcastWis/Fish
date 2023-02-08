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
            // �����Ǻ�������صĴ���
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
        // ����һ���ɼ̳еľ��, �����̱�����䲢��ʼ��һ��SECURITY_ATTRIBUTES�ṹ
        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(sa);
        sa.lpSecurityDescriptor = NULL;
        sa.bInheritHandle = TRUE;   // �ɼ̳�
        // ������һ��������, ���������ɼ̳еľ��
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

        // �򿪾���ļ̳�����
        SetHandleInformation(hObj, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
        // �رռ̳�����
        SetHandleInformation(hObj, HANDLE_FLAG_INHERIT, 0);
        
        // ��ȡ�������
        DWORD dwFlags;
        GetHandleInformation(hObj, &dwFlags);
        // �жϾ���Ƿ�ɼ̳�
        BOOL fHandleInInheritable = (0 != (dwFlags & HANDLE_FLAG_INHERIT));

        // ����ϵͳ������رվ��
        SetHandleInformation(hObj, HANDLE_FLAG_PROTECT_FROM_CLOSE, HANDLE_FLAG_PROTECT_FROM_CLOSE);
        try {
            CloseHandle(hObj);  // ������������, �������쳣. ����ֻ�᷵��FALSE
        }
        catch (...) {}

        SetHandleInformation(hObj, HANDLE_FLAG_PROTECT_FROM_CLOSE, 0);
        CloseHandle(hObj);

        return _T("");
    }

    CString NamedObject()
    {
        // �ܶഴ���ں˶���ĺ���, ������һ������ָ������. ���ƴ���NULL, ��ʾ������������.
        // �ں˶����������ȫ�ֵ�, ��û�л�����ʾ���Ƿ������ظ�.

        HANDLE hMutex = CreateMutex(NULL, FALSE, _T("John"));
        HANDLE hSem = CreateSemaphore(NULL, 1, 1, _T("John")); // ��������, �����Ͳ�һ��, ���´���ʧ��
        ASSERT(hSem == NULL);
        DWORD dwErrorCode = GetLastError(); // ERROR_INVALID_HANDLE
        ASSERT(dwErrorCode == ERROR_INVALID_HANDLE);


        // ����������һ��������ȥ��ȡ��������. ���������Ѵ���, ������Ҳ���ǻ�����, ����
        // �᷵����ǰ�����
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
        // ���1: ��Ҫ3�����̲���, Դ����S, Ŀ�����T, �м����C.
        {
            // Դ����1�����2, ���ɽ���C����DuplicateHandle, ���Ƹ�Ŀ�����3. hObj���ܾ����Ŀ������е�ֵ.
            // TRUE��ʾ�ɼ̳�, DUPLICATE_SAME_ACCESS����Ŀ������Դ���������ͬ��������
            HANDLE hObj = NULL;
            DuplicateHandle((HANDLE)1, (HANDLE)2, (HANDLE)3, &hObj, 0, TRUE, DUPLICATE_SAME_ACCESS);
        }

        // ���2: �漰2������. Դ����S��������Ƹ�Ŀ�����T
        {
            // ����һ��Դ�����µĶ�����
            HANDLE hObjInProcessS = CreateMutex(NULL, FALSE, NULL);
            // ��ȡpidΪ1�Ľ��̵Ľ��̾��, ��ΪĿ�����
            HANDLE hProcessT = OpenProcess(PROCESS_ALL_ACCESS, FALSE, 1);

            // ����. ʹ��Ŀ�����TҲ�ɷ��ʵ��½��̵Ļ��������
            HANDLE hObjInProcessT = NULL;
            DuplicateHandle(GetCurrentProcess(), hObjInProcessS, hProcessT, &hObjInProcessT, 0,
                FALSE, DUPLICATE_SAME_ACCESS);

            // ... do something

            // no longer need to commuicate with Process T
            CloseHandle(hProcessT);

            //
            CloseHandle(hObjInProcessS);
        }

        // ���3: ͬһ������, �����һ����ͬȨ�޵ľ��
        {
            // ����һ����дȨ�޵ľ��
            HANDLE hFileMapRW = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 10240, NULL);

            // ������һ��ֻ�ж�Ȩ�޵ľ��
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