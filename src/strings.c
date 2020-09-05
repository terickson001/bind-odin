#include "strings.h"

String make_string_alloc(gbAllocator alloc, char *str)
{
    String ret;
    ret.len = gb_strlen(str);
    ret.start = gb_alloc_str_len(alloc, str, ret.len);
    return ret;
}

String make_string_allocn(gbAllocator alloc, char *str, int n)
{
    String ret;
    ret.len = gb_strnlen(str, n);
    ret.start = gb_alloc_str_len(alloc, str, ret.len);
    return ret;
}

String make_string(char *str)
{
    return (String){str, gb_strlen(str)};
}

String make_stringn(char *str, int n)
{
    return (String){str, gb_strnlen(str, n)};
}

String empty_string()
{
    return (String){0, 0};
}

String alloc_string(String str)
{
    String ret;
    ret.len = str.len;
    ret.start = gb_alloc_str_len(gb_heap_allocator(), str.start, ret.len);
    return ret;
}

char *make_cstring(gbAllocator alloc, String str)
{
    return gb_alloc_str_len(alloc, str.start, str.len);
}

int string_cmp(String a, String b)
{
    if (!a.start || !b.start)
        return 1;
    return gb_strncmp(a.start, b.start, gb_max(a.len, b.len));
}

int cstring_cmp(String a, char const *b)
{
    if (a.len != gb_strlen(b))
        return 1;
    return gb_strncmp(b, a.start, a.len);
}

// TODO: Rename?
int csubstring_cmp(String a, char const *b)
{
    int blen = gb_strlen(b);
    if (a.len < blen)
        return 1;
    return gb_strncmp(b, a.start, blen);
}

b32 has_substring(String str, String substr)
{
    for (int i = 0; i < str.len-substr.len+1; i++)
    {
        int j;
        for (j = 0; j < substr.len; j++)
        {
            if (str.start[i+j] != substr.start[j])
                break;
        }
        if (j == substr.len)
            return true;
    }

    return false;
}

b32 has_prefix(String str, String prefix)
{
    if (prefix.len > str.len)
        return false;
    return gb_strncmp(str.start, prefix.start, prefix.len) == 0;
}

String string_slice(String str, int start, int end)
{
    if (start < 0) start = 0;
    if (end < 0)   end   = str.len;
    return (String){str.start+start, end-start};
}

i32 str_last_occurence(String str, char c)
{
    for (int i = str.len-1; i >= 0; i--)
        if (str.start[i] == c) return i;
    return -1;
}

String str_path_base_name(String filename)
{
    i32 ls;
    i32 ld;
    GB_ASSERT_NOT_NULL(filename.start);
    ls = str_last_occurence(filename, GB_PATH_SEPARATOR);
    ld = str_last_occurence(filename, '.');

    return string_slice(filename, ls+1, ld);
}

String path_base_name(char *filename)
{
    char *ls;
    char *ld;
    GB_ASSERT_NOT_NULL(filename);
    ls = (char *)gb_char_last_occurence(filename, GB_PATH_SEPARATOR);
    ld = (char *)gb_char_last_occurence(filename, '.');

    String ret = {0, 0};
    if (ls)
    {
        ret.start = ls+1;
        if (ld)
            ret.len = ld - ret.start;
        else
            ret.len = gb_strlen(ret.start);
    }
    else
    {
        ret.start = filename;
        if (ld)
            ret.len = ld - filename;
        else
            ret.len = gb_strlen(ret.start);
    }

    return ret;
}

String path_file_name(char *path)
{
    char *ls;
    GB_ASSERT_NOT_NULL(path);
    ls = (char *)gb_char_last_occurence(path, GB_PATH_SEPARATOR);

    String ret = {0, 0};
    if (ls)
    {
        ret.start = ls+1;
        ret.len = gb_strlen(ret.start);
    }
    else
    {
        ret.start = path;
        ret.len = gb_strlen(ret.start);
    }

    return ret;
}

String str_path_file_name(String filename)
{
    GB_ASSERT_NOT_NULL(filename.start);
    i32 ls = str_last_occurence(filename, GB_PATH_SEPARATOR);

    return string_slice(filename, ls+1, -1);
}

void normalize_path(String path)
{
    char from = GB_PATH_SEPARATOR=='/'?'\\':'/';
    for (int i = 0; i < path.len; i++)
        if (path.start[i] == from)
        path.start[i] = GB_PATH_SEPARATOR;
}

String dir_from_path(String path)
{
    String dir = {path.start, 0};
    char *slash = path.start+path.len-1;
    while (*slash != GB_PATH_SEPARATOR && slash > dir.start)
        slash--;
    dir.len = slash-dir.start+1;

    return dir;
}

