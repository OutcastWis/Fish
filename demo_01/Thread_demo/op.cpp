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
            // ���hThreadParam��α���, ��GetThreadTimes��õ��������߳�, �����Ǹ��̵߳�
            GetThreadTimes(hThreadParent, &ftCreationTime, &ftExitTime, &ftKernekTime, &ftUserTime);
            CloseHandle(hThreadParent);

            return 0;
        }

        DWORD WINAPI ChildThread2(PVOID pvParam)
        {
            // �ں�ʱ��, ��ʾ�߳�ִ���ں�ģʽ�µĲ���ϵͳ��������ʱ��ľ���ֵ, ��100nsΪ��λ
            FILETIME ftKernelTimeStart, ftKernelTimeEnd;
            // �û�ʱ��, һ���߳�ִ�д�������ʱ�����ֵ, ��100nsΪ��λ
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
        // c/c++ ʹ��_beginthreadex, _endthreadex

        // Windowsʹ�� CreateThread, ExitThread
    }

    void PseudoHandle()
    {
        HANDLE hThreadParent;
        // ͨ�����ƾ��, ��GetCurrentThread()��α���ת��Ϊ��ľ��
        DuplicateHandle(GetCurrentProcess(), 
            GetCurrentThread(),
            GetCurrentProcess(), 
            &hThreadParent, 
            0, FALSE, DUPLICATE_SAME_ACCESS);
        // Ҳ�����ڽ����̵�α���ת��������
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
        // ��detailChildThread������ľ��, ������α���
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
        // �鿴���ƼĴ����������Ĵ���
        context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;

        // ��ȡ. ��Ҫ��ͣ�߳�
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