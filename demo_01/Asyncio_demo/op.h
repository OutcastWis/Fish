#pragma once

namespace op
{
    // ����첽����
    void PushAsyncIO();
    
    // �����ں��豸����. �����Ƕ�hFile���еȴ�, �����ڶ��I/Oʱ, �޷��ж�����һ��������hFile
    void DealAsyncIO_1();

    // �����¼��ں˶���. ʹ��OVERLAPPED�е�hEvent�����I/O����
    void DealAsyncIO_2();

    // ������I/O. ������, ����
    void DealAsyncIO_3();

    // I/O��ɶ˿�
    void DealAsyncIO_4();
}