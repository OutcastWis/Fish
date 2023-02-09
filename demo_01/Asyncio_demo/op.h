#pragma once

namespace op
{
    // 添加异步请求
    void PushAsyncIO();
    
    // 触发内核设备对象. 由于是对hFile进行等待, 所以在多个I/O时, 无法判断是哪一个触发了hFile
    void DealAsyncIO_1();

    // 触发事件内核对象. 使用OVERLAPPED中的hEvent处理多I/O请求
    void DealAsyncIO_2();

    // 可提醒I/O. 不好用, 别用
    void DealAsyncIO_3();

    // I/O完成端口
    void DealAsyncIO_4();
}