#pragma once


namespace op
{
    namespace sync
    {
        // 一些处理器信息
        void CacheLineInfo();

        // 用户模式下的同步
        // 怎么用Interlocked*函数
        void UseInterlocked();
        // 关键段
        void CriticalSection();
        // Slim Read/Write
        void SlimRW();
        // 条件变量
        void ConditionVariable();

        // 上述性能对比
        void Performance();


        // 内核模式下的同步. 每一函数都需要从用户模式切换到内核模式
        // 事件
        void Event();
        // 可等待的计时器
        void WaitableTimer();
        // 信号量
        void Semaphore();
        // 互斥量
        void Mutex();
    }
}