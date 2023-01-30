#include "stdafx.h"
#include "op.h"

int main()
{
    // 显示时间, 100ns为底
    op::CalcThreadTime();
    // 显示上下文
    op::SeeThreadContext();
    return 0;
}