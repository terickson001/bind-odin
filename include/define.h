#ifndef C_PREPROCESSOR_DEFINE_H
#define C_PREPROCESSOR_DEFINE_H 1

#include "gb/gb.h"
#include "types.h"
#include "tokenizer.h"

typedef struct Define
{
    b32 in_use;
    String key;
    
    Token_Run value;
    gbArray(Token_Run) params;
    
    String file;
    isize line;
} Define;

GB_TABLE_DECLARE(, Define_Map, defines_, Define);

void add_define(Define_Map **defines, String name, Token_Run value, gbArray(Token_Run) params, isize line, String file);
void add_fake_define(Define_Map **defines, String name);
void remove_define(Define_Map **defines, String name);
Define *get_define(Define_Map *defines, String name);
void init_std_defines(Define_Map **defines);
gbArray(Define) get_define_list(Define_Map *defines, String whitelist_dir, gbAllocator alloc);

#endif /* ifndef C_PREPROCESSOR_DEFINE_H */
