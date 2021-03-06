#ifndef _C_PARSE_H_
#define _C_PARSE_H_

#include "tokenizer.h"
#include "ast.h"
#include "hashmap.h"
#include "define.h"

gb_global Node NODE_INVALID =
{
    .kind = NodeKind_Invalid,
    .index = 0
};

typedef struct Parser
{
    Token *start, *curr, *end;
    gbAllocator alloc;
    int node_index;

    b32 no_backtrack;

    map_t type_table;
    map_t opaque_types;
    
    Ast_File file;
} Parser;

Parser make_parser();
void destroy_parser(Parser p);
void parse_file(Parser *p);
void parse_defines(Parser *p, gbArray(Define) defines);


#endif
