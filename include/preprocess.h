#ifndef C_PREPROCESSOR_PREPROCESS_H
#define C_PREPROCESSOR_PREPROCESS_H 1

#include "gb/gb.h"
#include "types.h"
#include "define.h"
#include "tokenizer.h"
#include "parse_common.h"
#include "config.h"
#include "hashmap.h"

typedef struct Cond_Stack
{
    struct Cond_Stack *next;
    b32 skip_else;
} Cond_Stack;

typedef struct PP_Context
{
    struct PP_Context *next;
    
    Token_Run tokens;
	Token *prev_token;
    
    // location relative to beginning of file
    isize line;   // starts at 1
    isize column; // starts at 0
    String filename;
    
    b32 in_macro;
    Define macro;
    Token *preceding_token;
    
    b32 no_paste;
    
    b32 in_include;
    
    String from_filename;
    isize from_line;
    isize from_column;
    
    b32 stringify;
    
    b32 in_sandbox;
    
    Define_Map *local_defines;
} PP_Context;

typedef struct Preprocessor
{
    gbAllocator allocator;
    PreprocessorConfig *conf;
    
    gbArray(char *) file_contents;
    gbArray(Token *) file_tokens;
    
    PP_Context *context;
    
    Token *end_of_prev;
    
    String root_dir;
    isize line;
    isize column;
    
    isize write_line;
    isize write_column;
    
    Cond_Stack *conditionals;
    
    Define_Map *defines;
    
    gbArray(String) system_includes;
    
    gbArray(Token) output;
    
    b32 stringify_next;
    b32 paste_next;
    
    map_t pragma_onces;
} Preprocessor;

Preprocessor *make_preprocessor(gbArray(Token) tokens, String root_dir, String filename, PreprocessorConfig *conf);
void destroy_preprocessor(Preprocessor *pp);

void run_pp(Preprocessor *pp);
Define pp_get_define(Preprocessor *pp, String name);

gbArray(Token) pp_do_sandboxed_macro(Preprocessor *pp, Token_Run *run, Define define, Token name);
gbArray(Define) pp_dump_defines(Preprocessor *pp, String whitelist_dir);

void pp_print(Preprocessor *pp, char *filename);

#endif /* ifndef C_PREPROCESSOR_PREPROCESS_H */
