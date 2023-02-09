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

        // 对IO完成端口的简单封装
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
            // 将CreateIoCompletionPort分成了概念上的2步. 
            // 第一步, 创建一个nMaxConcurrency线程的完成端口
            BOOL Create(int nMaxConcurrency = 0)
            {
                m_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, nMaxConcurrency);
                if (m_hIOCP == NULL)
                    PrintString(GetErrorMsg());
                return m_hIOCP != NULL;
            }
            // 第二步, 将完成端口和设备关联.
            // 俩个函数可以用一步完成: CreateIoCompletionPort(hDevice, NULL, CompKey, nMaxConcurrency)
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

            // 向完成端口的I/O完成队列(先入先出)添加一条
            BOOL PostStatus(ULONG_PTR CompKey, DWORD dwNumBytes = 0, OVERLAPPED* po = NULL) {
                BOOL fOk = PostQueuedCompletionStatus(m_hIOCP, dwNumBytes, CompKey, po);
                if (!fOk)
                    PrintString(GetErrorMsg());
                return fOk;
            }

            // I/O完成端口的等待线程队列是后入先出的
            BOOL GetStatus(ULONG_PTR* pCompKey, PDWORD pdwNumBytes, OVERLAPPED** ppo, DWORD dwMillseconds = INFINITE) {
                return GetQueuedCompletionStatus(m_hIOCP, pdwNumBytes, pCompKey, ppo, dwMillseconds);
            }

        private:
            HANDLE m_hIOCP;
        };

        // 自定义的I/O请求
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
        * 结构解释
        typedef struct _OVERLAPPED {
            ULONG_PTR Internal;             // Error code
            ULONG_PTR InternalHigh;         // 异步I/O完成时, 记录已传输的字节数
            union {
                struct {
                    DWORD Offset;           // Low 32-bit file offset
                    DWORD OffsetHigh;       // High 32-bit file offet. Those two param should 0 when used on non-file device
                } DUMMYSTRUCTNAME;
                PVOID Pointer;
            } DUMMYUNIONNAME;

            HANDLE  hEvent;                 // 使用IO完成端口时使用的成员. 用可提醒I/O通知时, 按自己需求使用该成员
        } OVERLAPPED, * LPOVERLAPPED; OVERLAPPED
        */

        // ReadFile和WriteFile在同步时, 以返回值TRUE表示成功, FALSE表示失败. 但在异步时, 略有不同.
        // 异步下, 返回FALSE时, 需要调用GetLastError进行判断;
        OVERLAPPED o = { 0 };
        BYTE bBuffer[100] = {};
        HANDLE hFile; // 创建的异步句柄
        auto ret = WriteFile(hFile, bBuffer, 100, NULL, &o);
        if (ret == FALSE)
        {
            DWORD err = GetLastError();
            switch (err)
            {
            case ERROR_IO_PENDING:
                // 正常. 异步I/O请求已被加入队列
                break;
            case ERROR_INVALID_USER_BUFFER:
            case ERROR_NOT_ENOUGH_MEMORY:
                // 错误. 设备驱动程序的请求队列满了. 返回二者其一, 具体取决于驱动程序
                break;
            case ERROR_NOT_ENOUGH_QUOTA:
                // 系统没有足够的存储器页面可用
                break;
            default:
                break;
            }
        }

        // 取消添加到队列的I/O请求. CancelIo或CancelIoEx
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
        // 使用ReadFileEx, WriteFileEx. 请求完成后, 会向线程APC队列中添加一项

        // 线程可以通过SleepEx, WaitForSingleObjectEx, WaitForMultipleObjectEx, SingalObectAndWait,
        // GetQueuedCompletionStarusEx, MsgWaitForMultipleObjectEx使自己为可提醒状态

        // 可提醒状态下, 运行APC队列中每一项的回调函数. 
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

        const int BUFFSIZE = 64; // 当句柄用FILE_FLAG_NO_BUFFERING打开时, 访问时需要自己维护偏移量为磁盘扇区大小
        const int CK_READ = 1;
        const int CK_WRITE = 2;
        const int MAX_PENDING_IO_REQS = 4; // 最多同一时刻保持4个待处理I/O请求

        try
        {
            FileWrapper hFileSrc = CreateFile(srcFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, NULL);
            if (!hFileSrc.IsValid()) return;

            // 目标文件和源文件有一样的文件属性
            FileWrapper hFileDest = CreateFile(destFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, hFileSrc.hf_);
            if (!hFileDest.IsValid()) return;

            LARGE_INTEGER liFileSizeSrc = { 0 }, liFileSizeDst;
            GetFileSizeEx(hFileSrc.hf_, &liFileSizeSrc);    // 获取源文件大小
            // 当句柄用FILE_FLAG_NO_BUFFERING打开时, 访问时需要自己维护偏移量为磁盘扇区大小
            liFileSizeDst.QuadPart = ((liFileSizeSrc.QuadPart - 1) / BUFFSIZE + 1) * BUFFSIZE;
            // 要设置大小. 否则文件系统在扩充文件的时候是一同步方式进行的, 这样就不能使用异步写了
            SetFilePointerEx(hFileDest.hf_, liFileSizeDst, NULL, FILE_BEGIN);
            SetEndOfFile(hFileDest.hf_);

            // I/O完成端口
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
                if (!ior[i].AllocBuffer(BUFFSIZE)) // 设置异步读写buff大小为BUFFSIZE
                    detail::PrintString(detail::GetErrorMsg());
                ++nWriteInProgress;
                iocp.PostStatus(CK_WRITE, 0, &ior[i]); // 放入空的写, 触发读
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