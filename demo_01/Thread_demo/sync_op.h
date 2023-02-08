#pragma once


namespace op
{
    namespace sync
    {
        // һЩ��������Ϣ
        void CacheLineInfo();

        // �û�ģʽ�µ�ͬ��
        // ��ô��Interlocked*����
        void UseInterlocked();
        // �ؼ���
        void CriticalSection();
        // Slim Read/Write
        void SlimRW();
        // ��������
        void ConditionVariable();

        // �������ܶԱ�
        void Performance();


        // �ں�ģʽ�µ�ͬ��. ÿһ��������Ҫ���û�ģʽ�л����ں�ģʽ
        // �¼�
        void Event();
        // �ɵȴ��ļ�ʱ��
        void WaitableTimer();
        // �ź���
        void Semaphore();
        // ������
        void Mutex();
    }
}