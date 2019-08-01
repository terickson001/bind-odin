#ifndef _BIND_STRINGS_H_
#define _BIND_STRINGS_H_

#include "gb/gb.h"

#define LIT(x) ((int)(x).len), (x).start

typedef struct String
{
    char *start;
    int len;
} String;

String make_string_alloc(gbAllocator alloc, char *str);
String make_string(char *str);
String alloc_string(String str);
String empty_string();
char *make_cstring(gbAllocator alloc, String str);
int string_cmp(String a, String b);
int cstring_cmp(String a, char const *b);
int csubstring_cmp(String a, char const *b);
b32 has_substring(String str, String substr);
b32 has_prefix(String str, String prefix);
String string_slice(String str, int start, int end);
i32 str_last_occurence(String str, char c);
String str_path_base_name(String filename);
String path_base_name(char *filename);
void normalize_path(String path);
String dir_from_path(String path);

#endif
