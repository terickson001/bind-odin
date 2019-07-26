#ifndef _FIND_VS_H_
#define _FIND_VS_H_

typedef struct Find_Result {
    int windows_sdk_version;   // Zero if no Windows SDK found.

    wchar_t *windows_sdk_root;
    wchar_t *windows_sdk_um_library_path;
    wchar_t *windows_sdk_ucrt_library_path;
    
    wchar_t *vs_exe_path;
    wchar_t *vs_library_path;
} Find_Result;

extern Find_Result find_visual_studio_and_windows_sdk();

#endif
