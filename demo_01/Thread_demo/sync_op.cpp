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
            LONG g_span = FALSE; // false表示资源未被使用, true表示使用
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
            InterlockedExchange(&detail::g_x, 2);   // g_x = 2, 并返回旧值
            // 64位下, InterlockedExchangePointer, 和InterlockedExchange一样, 但交换的是64位
            InterlockedCompareExchange(&detail::g_x, 1, 2); // g_x = g_x == 2 ? 1 : g_x, 返回旧值
            // InterlockedCompareExchangePointer同理

            {
                // 一种旋转锁. 不要在单CPU上使用, 很耗时. 旋转锁假定被保护的资源只会被占用一小段时间
                while (InterlockedExchange(&detail::g_span, TRUE) == TRUE) Sleep(0); // 也可将Sleep(0)换成SwitchToThread

                // ... access the resource

                // 释放资源
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
            // 高速缓存行大小
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
            // init. 在任意线程调用Enter前必须先初始化
            InitializeCriticalSection(&detail::g_cs);

            if (false) {
                // Microsoft把旋转锁加到了关键段中, 所以有另一种初始化方法, 指定旋转次数
                if (FALSE == InitializeCriticalSectionAndSpinCount(&detail::g_cs, 4000))
                    return; // 函数可能由于内部调试信息内存分配失败而返回false

                // 可以用下面函数, 修改旋转次数
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
            // 手动事件和自动事件区别在于:
            //    1. 手动事件触发时, 所有等待的线程都变为可调度的. 而自动事件触发时, 只有一个线程变为可调度的
            //    2. 线程等到自动事件后, 会将其变为非触发, 即不需要手动调用ResetEvent
            HANDLE hEvent1 = CreateEvent(NULL, TRUE, FALSE, NULL);  // 匿名, 手动的未触发事件
            HANDLE hEvent2 = CreateEvent(NULL, FALSE, FALSE, NULL); // 匿名, 自动的未触发事件

            SetEvent(hEvent1); // 使其变为触发
            ResetEvent(hEvent1); // 使其变为非触发
            PulseEvent(hEvent1);    // 等价于SetEvent后立马又ResetEvent

            CloseHandle(hEvent1);
            CloseHandle(hEvent2);
        }

        void WaitableTimer()
        {
            // 创建时总是处于未触发状态
            HANDLE hWt = CreateWaitableTimer(NULL, FALSE, NULL); // 匿名, 自动重置的可等待计时器

            // 设置触发. 触发时间为2008.1.1 13:00, 之后每6小时触发一次
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

            // 可以传如负值, 表示相对时间. 单位为100ns. lPeriod传入0表示只触发一次
            {
                LARGE_INTEGER li;
                li.QuadPart = -5 * 10 * 1000 * 1000; // 5s
                // 5s后触发, 且只触发一次
                SetWaitableTimer(hWt, &li, 0, NULL, NULL, FALSE);
            }

            // 取消设置
            CancelWaitableTimer(hWt);
            CloseHandle(hWt);
        }

        void Semaphore() {
            // CreateSemaphore

            // OpenSemaphore

            // 增加资源计数
            // ReleaseSemaphore
        }

        void Mutex()
        {
            // CreateMutex

            // 和其他内核对象不同, 可以重复获取, 但也要多次释放
            // ReleaseMutex
        }
    }
}