#ifndef _FIND_VS_H_
#define _FIND_VS_H_

typedef struct Find_Result {
    int windows_sdk_version;   // Zero if no Windows SDK found.

    wchar_t *windows_sdk_root;
    wchar_t *windows_sdk_shared_include_path;
    wchar_t *windows_sdk_um_include_path;
    wchar_t *windows_sdk_ucrt_include_path;
    
    wchar_t *vs_include_path;
} Find_Result;

extern Find_Result find_visual_studio_and_windows_sdk();

#endif
