#ifndef _WRAP_H_
#define _WRAP_H_

#include "hashmap.h"
#include "gb/gb.h"
#include "ast.h"
#include "resolve.h"
#include "print.h"

typedef struct Wrapper
{
     Ast_File curr_file;
     gbFile *out_file;
     Printer *printer;
     Package package;
     map_t rename_map;
     map_t bind_rename_map;
     WrapConfig *conf;
     
     i32 indent;
     
     gbAllocator allocator;
} Wrapper;

Wrapper make_wrapper(Resolver resolver, WrapConfig *conf);
void wrap_package(Wrapper p);

#endif
