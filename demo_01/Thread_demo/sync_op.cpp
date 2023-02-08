#include "stdafx.h"

#include <vector>

#include "sync_op.h"

namespace op
{
    namespace sync
    {
        class CStopwatch {
        public:
            CStopwatch() { QueryPerformanceFrequency(&m_liPerfFreq); Start(); }

            void Start() { QueryPerformanceCounter(&m_liPerfStart); }

            __int64 Now() const {
                LARGE_INTEGER liPerfNow;
                QueryPerformanceCounter(&liPerfNow);
                return (((liPerfNow.QuadPart - m_liPerfStart.QuadPart) * 1000) / m_liPerfFreq.QuadPart);
            }

            __int64 NowInMicro() const {
                LARGE_INTEGER liPerfNow;
                QueryPerformanceCounter(&liPerfNow);
                return (((liPerfNow.QuadPart - m_liPerfStart.QuadPart) * 1000000) / m_liPerfFreq.QuadPart);
            }
        private:
            LARGE_INTEGER m_liPerfFreq;
            LARGE_INTEGER m_liPerfStart;
        };

        namespace detail
        {
            LONG g_x = 0; //  a global variable shared by many threads
            LONG g_span = FALSE; // false��ʾ��Դδ��ʹ��, true��ʾʹ��
            CRITICAL_SECTION g_cs;
            SRWLOCK g_srw;
            HANDLE g_hMutex;

            volatile LONG gv_value = 999;

            // Helper function to count set bits in the processor mask.
            DWORD CountSetBits(ULONG_PTR bitMask)
            {
                DWORD LSHIFT = sizeof(ULONG_PTR) * 8 - 1;
                DWORD bitSetCount = 0;
                ULONG_PTR bitTest = (ULONG_PTR)1 << LSHIFT;
                DWORD i;

                for (i = 0; i <= LSHIFT; ++i)
                {
                    bitSetCount += ((bitMask & bitTest) ? 1 : 0);
                    bitTest /= 2;
                }

                return bitSetCount;
            }

            // 'lValue': local variable is initialized but not referenced
#pragma warning(disable:4189)
            void WINAPI VolatileReadCallback()
            {
                LONG lValue = gv_value;
            }
#pragma warning(default:4189)

            void WINAPI VolatileWriteCallback()
            {
                gv_value = 0;
            }

            void WINAPI InterlockedIncrementCallback()
            {
                InterlockedIncrement(&gv_value);
            }

            void WINAPI CriticalSectionCallback()
            {
                EnterCriticalSection(&g_cs);
                gv_value = 0;
                LeaveCriticalSection(&g_cs);
            }

            void WINAPI SRWLockReadCallback() {
                AcquireSRWLockShared(&g_srw);
                gv_value = 0;
                ReleaseSRWLockShared(&g_srw);
            }

            void WINAPI SRWLockWriteCallback() {
                AcquireSRWLockExclusive(&g_srw);
                gv_value = 0;
                ReleaseSRWLockExclusive(&g_srw);
            }


            void WINAPI MutexCallback()
            {
                WaitForSingleObject(g_hMutex, INFINITE);
                gv_value = 0;
                ReleaseMutex(g_hMutex);
            }

            DWORD g_nIterations = 1000000;
            typedef void (CALLBACK* OPERATIONFUNC)();

            DWORD WINAPI ThreadIterationFunction(PVOID operationFunc) {
                OPERATIONFUNC op = (OPERATIONFUNC)operationFunc;
                for (DWORD iteration = 0; iteration < g_nIterations; iteration++) {
                    op();
                }
                return 0;
            }

            void MeasureConcurrentOperation(
                const TCHAR* operationName, DWORD nThreads, OPERATIONFUNC operationFunc) {
                HANDLE* phThreads = new HANDLE[nThreads];

                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
                for (DWORD currentThread = 0; currentThread < nThreads; currentThread++) {
                    phThreads[currentThread] =
                        CreateThread(NULL, 0, ThreadIterationFunction, operationFunc, 0, NULL);
                }
                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

                CStopwatch watch;
                WaitForMultipleObjects(nThreads, phThreads, TRUE, INFINITE);
                __int64 elapsedTime = watch.Now();
                _tprintf(
                    TEXT("Threads=%u, Milliseconds=%u, Test=%s\n"),
                    nThreads, (DWORD)elapsedTime, operationName);

                // Don't forget to clean up the thread handles
                for (DWORD currentThread = 0; currentThread < nThreads; currentThread++) {
                    CloseHandle(phThreads[currentThread]);
                }
                delete phThreads;
            }
        }

        void UseInterlocked()
        {
            InterlockedAdd(&detail::g_x, 1);        // g_x += 1
            InterlockedExchange(&detail::g_x, 2);   // g_x = 2, �����ؾ�ֵ
            // 64λ��, InterlockedExchangePointer, ��InterlockedExchangeһ��, ����������64λ
            InterlockedCompareExchange(&detail::g_x, 1, 2); // g_x = g_x == 2 ? 1 : g_x, ���ؾ�ֵ
            // InterlockedCompareExchangePointerͬ��

            {
                // һ����ת��. ��Ҫ�ڵ�CPU��ʹ��, �ܺ�ʱ. ��ת���ٶ�����������Դֻ�ᱻռ��һС��ʱ��
                while (InterlockedExchange(&detail::g_span, TRUE) == TRUE) Sleep(0); // Ҳ�ɽ�Sleep(0)����SwitchToThread

                // ... access the resource

                // �ͷ���Դ
                InterlockedExchange(&detail::g_span, FALSE);
            }
        }

        void CacheLineInfo()
        {
            BOOL done = FALSE;
            PSYSTEM_LOGICAL_PROCESSOR_INFORMATION buffer = NULL;
            PSYSTEM_LOGICAL_PROCESSOR_INFORMATION ptr = NULL;
            DWORD returnLength = 0;
            DWORD logicalProcessorCount = 0;
            DWORD numaNodeCount = 0;
            DWORD processorCoreCount = 0;
            DWORD processorL1CacheCount = 0;
            DWORD processorL2CacheCount = 0;
            DWORD processorL3CacheCount = 0;
            DWORD processorPackageCount = 0;
            DWORD byteOffset = 0;
            PCACHE_DESCRIPTOR Cache;
            // ���ٻ����д�С
            std::vector<WORD> l1CacheLineSize, l2CacheLineSize, l3CacheLineSize;

            while (!done)
            {
                DWORD rc = GetLogicalProcessorInformation(buffer, &returnLength);

                if (FALSE == rc)
                {
                    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
                    {
                        if (buffer)
                            free(buffer);

                        buffer = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)malloc(
                            returnLength);

                        if (NULL == buffer)
                        {
                            _tprintf(TEXT("\nError: Allocation failure\n"));
                            return;
                        }
                    }
                    else
                    {
                        _tprintf(TEXT("\nError %d\n"), GetLastError());
                        return;
                    }
                }
                else
                {
                    done = TRUE;
                }
            }

            ptr = buffer;

            while (byteOffset + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) <= returnLength)
            {
                switch (ptr->Relationship)
                {
                case RelationNumaNode:
                    // Non-NUMA systems report a single record of this type.
                    numaNodeCount++;
                    break;

                case RelationProcessorCore:
                    processorCoreCount++;

                    // A hyperthreaded core supplies more than one logical processor.
                    logicalProcessorCount += detail::CountSetBits(ptr->ProcessorMask);
                    break;

                case RelationCache:
                    // Cache data is in ptr->Cache, one CACHE_DESCRIPTOR structure for each cache. 
                    Cache = &ptr->Cache;
                    if (Cache->Level == 1)
                    {
                        processorL1CacheCount++;
                        l1CacheLineSize.push_back(Cache->LineSize);
                    }
                    else if (Cache->Level == 2)
                    {
                        processorL2CacheCount++;
                        l2CacheLineSize.push_back(Cache->LineSize);
                    }
                    else if (Cache->Level == 3)
                    {
                        processorL3CacheCount++;
                        l3CacheLineSize.push_back(Cache->LineSize);
                    }
                    break;

                case RelationProcessorPackage:
                    // Logical processors share a physical package.
                    processorPackageCount++;
                    break;

                default:
                    _tprintf(TEXT("\nError: Unsupported LOGICAL_PROCESSOR_RELATIONSHIP value.\n"));
                    break;
                }
                byteOffset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
                ptr++;
            }

            _tprintf(TEXT("\nGetLogicalProcessorInformation results:\n"));
            _tprintf(TEXT("Number of NUMA nodes: %d\n"),
                numaNodeCount);
            _tprintf(TEXT("Number of physical processor packages: %d\n"),
                processorPackageCount);
            _tprintf(TEXT("Number of processor cores: %d\n"),
                processorCoreCount);
            _tprintf(TEXT("Number of logical processors: %d\n"),
                logicalProcessorCount);
            _tprintf(TEXT("Number of processor L1/L2/L3 caches: %d/%d/%d\n"),
                processorL1CacheCount,
                processorL2CacheCount,
                processorL3CacheCount);

            CString info = TEXT("linesize of processor L1 caches:");
            for (auto i : l1CacheLineSize)
                info.AppendFormat(TEXT("%d, "), i);
            _tprintf(TEXT("%s\n"), static_cast<const TCHAR*>(info));
            info = TEXT("linesize of processor L2 caches:");
            for (auto i : l2CacheLineSize)
                info.AppendFormat(TEXT("%d, "), i);
            _tprintf(TEXT("%s\n"), static_cast<const TCHAR*>(info));
            info = TEXT("linesize of processor L3 caches:");
            for (auto i : l3CacheLineSize)
                info.AppendFormat(TEXT("%d, "), i);
            _tprintf(TEXT("%s\n"), static_cast<const TCHAR*>(info));

            free(buffer);
        }

        void CriticalSection()
        {
            // init. �������̵߳���Enterǰ�����ȳ�ʼ��
            InitializeCriticalSection(&detail::g_cs);

            if (false) {
                // Microsoft����ת���ӵ��˹ؼ�����, ��������һ�ֳ�ʼ������, ָ����ת����
                if (FALSE == InitializeCriticalSectionAndSpinCount(&detail::g_cs, 4000))
                    return; // �������������ڲ�������Ϣ�ڴ����ʧ�ܶ�����false

                // ���������溯��, �޸���ת����
                SetCriticalSectionSpinCount(&detail::g_cs, 5000);
            }

            // enter or try-enter
            EnterCriticalSection(&detail::g_cs); // TryEnterCriticalSection(&detail::g_cs)

            // leave
            LeaveCriticalSection(&detail::g_cs);

            // delete
            DeleteCriticalSection(&detail::g_cs);
        }

        void SlimRW()
        {
            InitializeSRWLock(&detail::g_srw);

            {
                // write enter
                AcquireSRWLockExclusive(&detail::g_srw);
                // write leave
                ReleaseSRWLockExclusive(&detail::g_srw);
            }

            {
                // read enter
                AcquireSRWLockShared(&detail::g_srw);
                // read leave
                ReleaseSRWLockShared(&detail::g_srw);
            }

            // no need to delete g_srw, it will be cleared by system
        }

        void Performance()
        {
            for (int i = 1; i <= 8; i *= 2)
            {
                detail::MeasureConcurrentOperation(TEXT("Volatile Read"), i, detail::VolatileReadCallback);
                detail::MeasureConcurrentOperation(TEXT("Volatile Write"), i, detail::VolatileWriteCallback);
                detail::MeasureConcurrentOperation(TEXT("Interlocked Increment"), i, detail::InterlockedIncrementCallback);

                // Prepare the critical section
                InitializeCriticalSection(&detail::g_cs);
                detail::MeasureConcurrentOperation(TEXT("Critical Section"), i, detail::CriticalSectionCallback);
                // Don't forget to cleanup
                DeleteCriticalSection(&detail::g_cs);

                // Prepare the Slim Reader/Writer lock
                InitializeSRWLock(&detail::g_srw);
                detail::MeasureConcurrentOperation(TEXT("SRWLock Read"), i, detail::SRWLockReadCallback);
                detail::MeasureConcurrentOperation(TEXT("SRWLock Write"), i, detail::SRWLockWriteCallback);
                // NOTE: You can't cleanup a Slim Reader/Writer lock

                // Prepare the mutex
                detail::g_hMutex = CreateMutex(NULL, false, NULL);
                detail::MeasureConcurrentOperation(TEXT("Mutex"), i, detail::MutexCallback);
                CloseHandle(detail::g_hMutex);
                _tprintf(TEXT("\n"));
            }
        }

        void ConditionVariable()
        {
            // SleepConditionVariableCS

            // SleepConditionVariableSRW

            // WakeConditionVariable

            // WakeAllConditionVariable
        }

        void Event()
        {
            // �ֶ��¼����Զ��¼���������:
            //    1. �ֶ��¼�����ʱ, ���еȴ����̶߳���Ϊ�ɵ��ȵ�. ���Զ��¼�����ʱ, ֻ��һ���̱߳�Ϊ�ɵ��ȵ�
            //    2. �̵߳ȵ��Զ��¼���, �Ὣ���Ϊ�Ǵ���, ������Ҫ�ֶ�����ResetEvent
            HANDLE hEvent1 = CreateEvent(NULL, TRUE, FALSE, NULL);  // ����, �ֶ���δ�����¼�
            HANDLE hEvent2 = CreateEvent(NULL, FALSE, FALSE, NULL); // ����, �Զ���δ�����¼�

            SetEvent(hEvent1); // ʹ���Ϊ����
            ResetEvent(hEvent1); // ʹ���Ϊ�Ǵ���
            PulseEvent(hEvent1);    // �ȼ���SetEvent��������ResetEvent

            CloseHandle(hEvent1);
            CloseHandle(hEvent2);
        }

        void WaitableTimer()
        {
            // ����ʱ���Ǵ���δ����״̬
            HANDLE hWt = CreateWaitableTimer(NULL, FALSE, NULL); // ����, �Զ����õĿɵȴ���ʱ��

            // ���ô���. ����ʱ��Ϊ2008.1.1 13:00, ֮��ÿ6Сʱ����һ��
            {
                SYSTEMTIME st;
                FILETIME ftLocal, ftUTC;
                LARGE_INTEGER liUTC;

                st.wYear = 2008, st.wMonth = 1, st.wDay = 1;
                st.wDayOfWeek = 0; // ignore
                st.wHour = 13, st.wMinute = 0, st.wSecond = 0, st.wMilliseconds = 0;
                SystemTimeToFileTime(&st, &ftLocal);

                // Convert local time to UTC time
                LocalFileTimeToFileTime(&ftLocal, &ftUTC);
                // Convert FILETIME to LARGE_INTEGER because of differnet aligment
                liUTC.LowPart = ftUTC.dwLowDateTime;
                liUTC.HighPart = ftUTC.dwHighDateTime;

                SetWaitableTimer(hWt, &liUTC, 6 * 60 * 60 * 1000, NULL, NULL, FALSE);
            }

            // ���Դ��縺ֵ, ��ʾ���ʱ��. ��λΪ100ns. lPeriod����0��ʾֻ����һ��
            {
                LARGE_INTEGER li;
                li.QuadPart = -5 * 10 * 1000 * 1000; // 5s
                // 5s�󴥷�, ��ֻ����һ��
                SetWaitableTimer(hWt, &li, 0, NULL, NULL, FALSE);
            }

            // ȡ������
            CancelWaitableTimer(hWt);
            CloseHandle(hWt);
        }

        void Semaphore() {
            // CreateSemaphore

            // OpenSemaphore

            // ������Դ����
            // ReleaseSemaphore
        }

        void Mutex()
        {
            // CreateMutex

            // �������ں˶���ͬ, �����ظ���ȡ, ��ҲҪ����ͷ�
            // ReleaseMutex
        }
    }
}