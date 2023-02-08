/********************************************
Module: Dll_demo.h
********************************************/

// 隐式链接需要头文件


#ifdef DLLDEMO_EXPORTS
// 使用extern "C", 这样C编写的exe也能使用该dll.
// 含有导出的不能延迟加载
#define DLLDEMOAPI extern "C" __declspec(dllexport)
#else
#define DLLDEMOAPI extern "C" __declspec(dllimport)
#endif

// 定义数据结构和符号
namespace mystrcut{

}


// 定义要导出的C++类. 
// 注意: 只有导出c++类模块的编译器和导入c++类模块的编译器一样, 才能导出c++类.
//       否则, 应该避免导出c++类
namespace myclass
{

}

// 定义需要导出的函数
namespace myfunc
{
    DLLDEMOAPI int add(int x, int y);
}
DLLDEMOAPI int multi(int x, int y);

// 不要导出变量

