#include "pch.h"

#include "op.h"

// ����ת��.ҪΪÿ����Ҫת���ĺ���дһ��pragma
// ���һ��Wzj�ĺ���, ��ʵ��ָ��OtherDll�е�Func����
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