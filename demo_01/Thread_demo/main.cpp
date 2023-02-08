#include "stdafx.h"

#include <iostream>

#include "op.h"
#include "sync_op.h"

int main()
{
    // 显示线程执行时间, 100ns为底
    op::CalcThreadTime();
    std::cout << "===============================" << std::endl;
    // 显示上下文
    op::SeeThreadContext();
    std::cout << "===============================" << std::endl;
    // 显示处理器信息
    op::sync::CacheLineInfo();
    std::cout << "===============================" << std::endl;
    // 性能
    Sleep(5000);
    op::sync::Performance();
    std::cout << "===============================" << std::endl;

    return 0;
}