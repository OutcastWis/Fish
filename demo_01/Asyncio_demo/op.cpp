#include "stdafx.h"

#include <iostream>
#include <string>
#include <cstdio>

#include "op.h"

namespace op
{
    namespace detail
    {
        CString PrintString(const CString& str)
        {
            OutputDebugString(str);
            _tprintf(_T("%s\n"), static_cast<const TCHAR*>(str));
        }

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

        // ��IO��ɶ˿ڵļ򵥷�װ
        class CIOCP
        {
        public:
            CIOCP(int nMaxConCurrency = -1)
            {
                m_hIOCP = NULL;
                if (nMaxConCurrency != -1)
                    Create(nMaxConCurrency);
            }

            ~CIOCP() {
                if (m_hIOCP != NULL)
                {
                    if (!CloseHandle(m_hIOCP))
                        PrintString(GetErrorMsg());
                }
            }

            BOOL Close() {
                BOOL bResult = CloseHandle(m_hIOCP);
                m_hIOCP = NULL;
                return bResult;
            }
            // ��CreateIoCompletionPort�ֳ��˸����ϵ�2��. 
            // ��һ��, ����һ��nMaxConcurrency�̵߳���ɶ˿�
            BOOL Create(int nMaxConcurrency = 0)
            {
                m_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, nMaxConcurrency);
                if (m_hIOCP == NULL)
                    PrintString(GetErrorMsg());
                return m_hIOCP != NULL;
            }
            // �ڶ���, ����ɶ˿ں��豸����.
            // ��������������һ�����: CreateIoCompletionPort(hDevice, NULL, CompKey, nMaxConcurrency)
            BOOL AssociateDevice(HANDLE hDevice, ULONG_PTR CompKey)
            {
                BOOL fOK = (CreateIoCompletionPort(hDevice, m_hIOCP, CompKey, 0) == m_hIOCP);
                if (!fOK)
                    PrintString(GetErrorMsg());
                return fOK;
            }

            BOOL AssociateSocket(SOCKET hSocket, ULONG_PTR CompKey)
            {
                return AssociateDevice((HANDLE)hSocket, CompKey);
            }

            // ����ɶ˿ڵ�I/O��ɶ���(�����ȳ�)���һ��
            BOOL PostStatus(ULONG_PTR CompKey, DWORD dwNumBytes = 0, OVERLAPPED* po = NULL) {
                BOOL fOk = PostQueuedCompletionStatus(m_hIOCP, dwNumBytes, CompKey, po);
                if (!fOk)
                    PrintString(GetErrorMsg());
                return fOk;
            }

            // I/O��ɶ˿ڵĵȴ��̶߳����Ǻ����ȳ���
            BOOL GetStatus(ULONG_PTR* pCompKey, PDWORD pdwNumBytes, OVERLAPPED** ppo, DWORD dwMillseconds = INFINITE) {
                return GetQueuedCompletionStatus(m_hIOCP, pdwNumBytes, pCompKey, ppo, dwMillseconds);
            }

        private:
            HANDLE m_hIOCP;
        };

        // �Զ����I/O����
        class CIOReq : public OVERLAPPED
        {
        public:
            CIOReq() :m_nBuffSize(0), m_pvData(NULL) {
                Internal = InternalHigh = 0;
                Offset = OffsetHigh = 0;
                hEvent = NULL;
            }

            ~CIOReq() {
                if (m_pvData != NULL)
                    VirtualFree(m_pvData, 0, MEM_RELEASE);
            }

            BOOL AllocBuffer(SIZE_T nBuffSize)
            {
                m_nBuffSize = nBuffSize;
                m_pvData = VirtualAlloc(NULL, m_nBuffSize, MEM_COMMIT, PAGE_READWRITE);
                return m_pvData != NULL;
            }

            BOOL Read(HANDLE hDevice, PLARGE_INTEGER pliOffset = NULL) {
                if (pliOffset != NULL)
                {
                    Offset = pliOffset->LowPart;
                    OffsetHigh = pliOffset->HighPart;
                }
                return ::ReadFile(hDevice, m_pvData, m_nBuffSize, NULL, this);
            }

            BOOL Write(HANDLE hDevice, PLARGE_INTEGER pliOffset= NULL) {
                if (pliOffset != NULL)
                {
                    Offset = pliOffset->LowPart;
                    OffsetHigh = pliOffset->HighPart;
                }
                return ::WriteFile(hDevice, m_pvData, m_nBuffSize, NULL, this);
            }
        private:
            SIZE_T m_nBuffSize;
            PVOID  m_pvData;
        };
    }


    void AsyncIO()
    {
        /**
        * �ṹ����
        typedef struct _OVERLAPPED {
            ULONG_PTR Internal;             // Error code
            ULONG_PTR InternalHigh;         // �첽I/O���ʱ, ��¼�Ѵ�����ֽ���
            union {
                struct {
                    DWORD Offset;           // Low 32-bit file offset
                    DWORD OffsetHigh;       // High 32-bit file offet. Those two param should 0 when used on non-file device
                } DUMMYSTRUCTNAME;
                PVOID Pointer;
            } DUMMYUNIONNAME;

            HANDLE  hEvent;                 // ʹ��IO��ɶ˿�ʱʹ�õĳ�Ա. �ÿ�����I/O֪ͨʱ, ���Լ�����ʹ�øó�Ա
        } OVERLAPPED, * LPOVERLAPPED; OVERLAPPED
        */

        // ReadFile��WriteFile��ͬ��ʱ, �Է���ֵTRUE��ʾ�ɹ�, FALSE��ʾʧ��. �����첽ʱ, ���в�ͬ.
        // �첽��, ����FALSEʱ, ��Ҫ����GetLastError�����ж�;
        OVERLAPPED o = { 0 };
        BYTE bBuffer[100] = {};
        HANDLE hFile; // �������첽���
        auto ret = WriteFile(hFile, bBuffer, 100, NULL, &o);
        if (ret == FALSE)
        {
            DWORD err = GetLastError();
            switch (err)
            {
            case ERROR_IO_PENDING:
                // ����. �첽I/O�����ѱ��������
                break;
            case ERROR_INVALID_USER_BUFFER:
            case ERROR_NOT_ENOUGH_MEMORY:
                // ����. �豸��������������������. ���ض�����һ, ����ȡ������������
                break;
            case ERROR_NOT_ENOUGH_QUOTA:
                // ϵͳû���㹻�Ĵ洢��ҳ�����
                break;
            default:
                break;
            }
        }

        // ȡ����ӵ����е�I/O����. CancelIo��CancelIoEx
    }

    void DealAsyncIO_1()
    {
        HANDLE hFile = CreateFile(_T("wzj_file"), GENERIC_READ, 0, NULL, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);
        
        BYTE bBuffer[100] = {};
        OVERLAPPED o = { 0 };
        o.Offset = 345;

        BOOL bReadDown = ReadFile(hFile, bBuffer, 100, NULL, &o);
        DWORD dwError = GetLastError();

        // do something

        if (!bReadDown && (dwError == ERROR_IO_PENDING))
        {
            // The I/O is being performed asynchronously, wait for it to complete
            WaitForSingleObject(hFile, INFINITE);
            bReadDown = TRUE;
        }

        if (bReadDown)
        {
            // do something
        }
        else {
            // An error occurred. see dwError
        }
    }

    void DealAsyncIO_2()
    {
        HANDLE hFile = CreateFile(_T("wzj_file"), GENERIC_READ, 0, NULL, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);

        BYTE bReadBuffer[100] = {};
        OVERLAPPED oRead = { 0 };
        oRead.Offset = 0;
        oRead.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        ReadFile(hFile, bReadBuffer, 100, NULL, &oRead);

        BYTE bWriteBuffer[100];
        OVERLAPPED oWrite = { 0 };
        oWrite.Offset = 100;
        oWrite.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        WriteFile(hFile, bWriteBuffer, 100 * sizeof(BYTE), NULL, &oWrite);

        // do something

        // wait
        {
            HANDLE h[2];
            h[0] = oRead.hEvent;
            h[1] = oWrite.hEvent;
            DWORD dw = WaitForMultipleObjects(2, h, FALSE, INFINITE);
            switch (dw - WAIT_OBJECT_0)
            {
            case 0: // Read completed
                break;
            case 1: // Write completed
                break;
            default:
                break;
            }
        }
    }

    void DealAsyncIO_3()
    {
        // ʹ��ReadFileEx, WriteFileEx. ������ɺ�, �����߳�APC���������һ��

        // �߳̿���ͨ��SleepEx, WaitForSingleObjectEx, WaitForMultipleObjectEx, SingalObectAndWait,
        // GetQueuedCompletionStarusEx, MsgWaitForMultipleObjectExʹ�Լ�Ϊ������״̬

        // ������״̬��, ����APC������ÿһ��Ļص�����. 
    }

    void DealAsyncIO_4()
    {
        TCHAR srcFile[MAX_PATH] = {}, destFile[MAX_PATH] = {};
        _tprintf(_T("Please input the source file:\n"));
        _tscanf(_T("%s"), srcFile);
        _tprintf(_T("Please input the destination file:\n"));
        _tscanf(_T("%s"), destFile);

        struct FileWrapper
        {
            FileWrapper(HANDLE hf) : hf_(hf) {}
            ~FileWrapper() { if (hf_ != INVALID_HANDLE_VALUE) CloseHandle(hf_); hf_ = INVALID_HANDLE_VALUE; }

            BOOL IsValid() const { return hf_ != INVALID_HANDLE_VALUE; }

            HANDLE hf_ = INVALID_HANDLE_VALUE;
        };

        const int BUFFSIZE = 64; // �������FILE_FLAG_NO_BUFFERING��ʱ, ����ʱ��Ҫ�Լ�ά��ƫ����Ϊ����������С
        const int CK_READ = 1;
        const int CK_WRITE = 2;
        const int MAX_PENDING_IO_REQS = 4; // ���ͬһʱ�̱���4��������I/O����

        try
        {
            FileWrapper hFileSrc = CreateFile(srcFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, NULL);
            if (!hFileSrc.IsValid()) return;

            // Ŀ���ļ���Դ�ļ���һ�����ļ�����
            FileWrapper hFileDest = CreateFile(destFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, hFileSrc.hf_);
            if (!hFileDest.IsValid()) return;

            LARGE_INTEGER liFileSizeSrc = { 0 }, liFileSizeDst;
            GetFileSizeEx(hFileSrc.hf_, &liFileSizeSrc);    // ��ȡԴ�ļ���С
            // �������FILE_FLAG_NO_BUFFERING��ʱ, ����ʱ��Ҫ�Լ�ά��ƫ����Ϊ����������С
            liFileSizeDst.QuadPart = ((liFileSizeSrc.QuadPart - 1) / BUFFSIZE + 1) * BUFFSIZE;
            // Ҫ���ô�С. �����ļ�ϵͳ�������ļ���ʱ����һͬ����ʽ���е�, �����Ͳ���ʹ���첽д��
            SetFilePointerEx(hFileDest.hf_, liFileSizeDst, NULL, FILE_BEGIN);
            SetEndOfFile(hFileDest.hf_);

            // I/O��ɶ˿�
            detail::CIOCP iocp(0);
            iocp.AssociateDevice(hFileSrc.hf_, CK_READ);
            iocp.AssociateDevice(hFileDest.hf_, CK_WRITE);

            // Initialize record-keeping variables
            detail::CIOReq ior[MAX_PENDING_IO_REQS];
            LARGE_INTEGER liNextReadOffset = { 0 };
            int nReadsInProgress = 0;
            int nWriteInProgress = 0;

            for (int i = 0; i < MAX_PENDING_IO_REQS; ++i)
            {
                if (!ior[i].AllocBuffer(BUFFSIZE)) // �����첽��дbuff��СΪBUFFSIZE
                    detail::PrintString(detail::GetErrorMsg());
                ++nWriteInProgress;
                iocp.PostStatus(CK_WRITE, 0, &ior[i]); // ����յ�д, ������
            }

            BOOL bResult = FALSE;
            while ((nReadsInProgress > 0) | (nWriteInProgress > 0))
            {
                ULONG_PTR CompltionKey;
                DWORD dwNumByte;
                detail::CIOReq* pior;
                bResult = iocp.GetStatus(&CompltionKey, &dwNumByte, (OVERLAPPED**)&pior);

                switch (CompltionKey)
                {
                case CK_READ:
                    --nReadsInProgress;
                    bResult = pior->Write(hFileDest.hf_);
                    ++nWriteInProgress;
                    break;
                case CK_WRITE:
                    --nWriteInProgress;
                    if (liNextReadOffset.QuadPart < liFileSizeDst.QuadPart)
                    {
                        bResult = pior->Read(hFileSrc.hf_, &liNextReadOffset);
                        liNextReadOffset.QuadPart += BUFFSIZE;
                        ++nReadsInProgress;
                    }
                    break;
                default:
                    break;
                }
            }
        }
        catch (...)
        {

        }
    }
}