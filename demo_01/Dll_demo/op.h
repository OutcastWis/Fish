/********************************************
Module: Dll_demo.h
********************************************/

// ��ʽ������Ҫͷ�ļ�


#ifdef DLLDEMO_EXPORTS
// ʹ��extern "C", ����C��д��exeҲ��ʹ�ø�dll.
// ���е����Ĳ����ӳټ���
#define DLLDEMOAPI extern "C" __declspec(dllexport)
#else
#define DLLDEMOAPI extern "C" __declspec(dllimport)
#endif

// �������ݽṹ�ͷ���
namespace mystrcut{

}


// ����Ҫ������C++��. 
// ע��: ֻ�е���c++��ģ��ı������͵���c++��ģ��ı�����һ��, ���ܵ���c++��.
//       ����, Ӧ�ñ��⵼��c++��
namespace myclass
{

}

// ������Ҫ�����ĺ���
namespace myfunc
{
    DLLDEMOAPI int add(int x, int y);
}
DLLDEMOAPI int multi(int x, int y);

// ��Ҫ��������

