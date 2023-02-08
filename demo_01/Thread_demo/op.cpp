#include "stdafx.h"

#include "op.h"

namespace op
{
    namespace detail
    {
        __int64 FileTimeToQuadWord(PFILETIME pft)
        {
            return (Int64ShllMod32(pft->dwHighDateTime, 32) | pft->dwLowDateTime);
        }

        DWORD WINAPI ChildThread(PVOID pvParam)
        {
            HANDLE hThreadParent = (HANDLE)pvParam;
            FILETIME ftCreationTime, ftExitTime, ftKernekTime, ftUserTime;
            // 如果hThreadParam是伪句柄, 则GetThreadTimes获得的是自身线程, 而不是父线程的
            GetThreadTimes(hThreadParent, &ftCreationTime, &ftExitTime, &ftKernekTime, &ftUserTime);
            CloseHandle(hThreadParent);

            return 0;
        }

        DWORD WINAPI ChildThread2(PVOID pvParam)
        {
            // 内核时间, 表示线程执行内核模式下的操作系统代码所用时间的绝对值, 以100ns为单位
            FILETIME ftKernelTimeStart, ftKernelTimeEnd;
            // 用户时间, 一个线程执行代码所用时间绝对值, 以100ns为单位
            FILETIME ftUserTimeStart, ftUserTimeEnd;
            //
            FILETIME ftDummy;
            // 
            __int64 qwKernelTimeElapsed, qwUserTimeElapsed, qwTotalTimeElapsed;

            // Get start times
            GetThreadTimes(GetCurrentThread(), &ftDummy, &ftDummy, &ftKernelTimeStart, &ftUserTimeStart);

            // Perform complex algorithm here
            int i = 1;
            for (int j = 0; j < 20000000; ++j)
                i += j * j % 987;

            // Get ending times
            GetThreadTimes(GetCurrentThread(), &ftDummy, &ftDummy, &ftKernelTimeEnd, &ftUserTimeEnd);

            qwKernelTimeElapsed = FileTimeToQuadWord(&ftKernelTimeEnd) - FileTimeToQuadWord(&ftKernelTimeStart);
            qwUserTimeElapsed = FileTimeToQuadWord(&ftUserTimeEnd) - FileTimeToQuadWord(&ftUserTimeStart);
            qwTotalTimeElapsed = qwKernelTimeElapsed + qwUserTimeElapsed;

            _tprintf(_T("Kernel: %lld,\nUser: %lld,\nTotal: %lld\n"), qwKernelTimeElapsed, qwUserTimeElapsed, qwTotalTimeElapsed);

            return 0;
        }
    }


    void UseCreateThread()
    {
        // c/c++ 使用_beginthreadex, _endthreadex

        // Windows使用 CreateThread, ExitThread
    }

    void PseudoHandle()
    {
        HANDLE hThreadParent;
        // 通过复制句柄, 将GetCurrentThread()的伪句柄转化为真的句柄
        DuplicateHandle(GetCurrentProcess(), 
            GetCurrentThread(),
            GetCurrentProcess(), 
            &hThreadParent, 
            0, FALSE, DUPLICATE_SAME_ACCESS);
        // 也可用于将进程的伪句柄转化成真句柄
        {
            HANDLE hProcess;
            DuplicateHandle(
                GetCurrentProcess(),
                GetCurrentProcess(),
                GetCurrentProcess(),
                &hProcess,
                0, FALSE, DUPLICATE_SAME_ACCESS);
            CloseHandle(hProcess);
        }
        // 向detailChildThread传入真的句柄, 而不是伪句柄
        CreateThread(NULL, 0, detail::ChildThread, (PVOID)hThreadParent, 0, NULL);
    }

    void CalcThreadTime()
    {
        HANDLE hThread = CreateThread(NULL, 0, detail::ChildThread2, NULL, 0, NULL);
        if (hThread == 0)
            return;
        
        SuspendThread(hThread);
        Sleep(5000);
        ResumeThread(hThread);

        CloseHandle(hThread);

        Sleep(5000);
    }

    void SeeThreadContext()
    {
        HANDLE hThread = CreateThread(NULL, 0, detail::ChildThread2, NULL, 0, NULL);
        if (hThread == 0)
            return;

        CONTEXT context;
        // 查看控制寄存器和整数寄存器
        context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;

        // 获取. 先要暂停线程
        SuspendThread(hThread);
        GetThreadContext(hThread, &context);

        // CONTEXT_CONTROL specifies SegSs, Rsp, SegCs, Rip, and EFlags.
        _tprintf(_T("Control register: ==>\n  SegSs=%d, Rsp=%lld, SegCs=%d,\n  Rip=%lld, EFlags=%d\n"), context.SegSs, context.Rsp,
            context.SegCs, context.Rip, context.EFlags);
        // CONTEXT_INTEGER specifies Rax, Rcx, Rdx, Rbx, Rbp, Rsi, Rdi, and R8-R15.
        // ...

        //
        ResumeThread(hThread);
        CloseHandle(hThread);
    }
}