#pragma once

#define WIN32_LEAN_AND_MEAN             // 从 Windows 头文件中排除极少使用的内容
// Windows 头文件
#include <windows.h>

#ifdef SHAREMEMORYDLL_EXPORTS
#define SHARE_MEMORY_DLLEXPORT __declspec(dllexport)
#else
#define SHARE_MEMORY_DLLEXPORT //__declspec(dllimport)
#endif
