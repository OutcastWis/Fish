#include "pch.h"

#include "op.h"

// 函数转发.要为每个需要转发的函数写一行pragma
// 输出一个Wzj的函数, 但实际指向OtherDll中的Func函数
#pragma comment(linker, "/export:Wzj=OtherDll.Func")

namespace myfunc
{
    int add(int x, int y) {
        return (x + y);
    }
}

int multi(int x, int y) {
    return (x * y);
}