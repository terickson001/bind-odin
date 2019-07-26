#ifndef _FIND_VS_H_
#define _FIND_VS_H_

#include
typedef struct Find_Result {
    int windows_sdk_version;   // Zero if no Windows SDK found.

    wchar_t *windows_sdk_root              = NULL;
    wchar_t *windows_sdk_um_library_path   = NULL;
    wchar_t *windows_sdk_ucrt_library_path = NULL;
    
    wchar_t *vs_exe_path = NULL;
    wchar_t *vs_library_path = NULL;
} Find_Result;

extern "C" Find_Result find_visual_studio_and_windwos_sdk();

#endif
