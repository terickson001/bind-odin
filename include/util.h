#ifndef _C_BIND_UTIL_H
#define _C_BIND_UTIL_H 1

#include "types.h"
#include "ast.h"
#include "hashmap.h"
#include "config.h"
#include "strings.h"

#include "gb/gb.h"

/* String helper functions */
Token_Run alloc_token_run(Token *tokens, int count);

void create_path_to_file(char const *filename);

/* Time helper functions */
char *date_string(u64 time);
char *time_string(u64 time); // TODO: Currently in UTC, change to local time

/* Integer parsing */
u64 str_to_int_base(String str, u64 base);
u64 str_to_int(String str);

/* Case conversion */

String convert_case(String str, Case case_type, gbAllocator allocator);
String to_snake_case(String str, gbAllocator allocator);
String to_ada_case(String str, gbAllocator allocator);
String to_screaming_snake_case(String str, gbAllocator allocator);

void init_rename_map(map_t rename_map, gbAllocator allocator);

typedef enum Rename_Kind {
    RENAME_TYPE,
    RENAME_VAR,
    RENAME_PROC,
    RENAME_CONST,
} Rename_Kind;
String rename_ident(String orig, Rename_Kind r, b32 do_remove_prefix, map_t rename_map, BindConfig *conf, gbAllocator allocator);

String float_type_str(Node *type);
String integer_type_str(Node *type);
String convert_type(Node *type, map_t rename_map, BindConfig *conf, gbAllocator allocator);

char *repeat_char(char c, int count, gbAllocator allocator);

b32 is_valid_ident(String ident);

gbArray(String) get_system_includes(gbAllocator a);

#endif /* ifndef _C_BIND_UTIL_H */
