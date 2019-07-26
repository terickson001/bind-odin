#ifndef C_PARSER_PRINT_H_
#define C_PARSER_PRINT_H_ 1

#include "ast.h"
#include "string.h"
#include "util.h"
#include "resolve.h"
#include "config.h"

typedef struct Printer
{
    
    Ast_File file;
    gbFile *out_file;
    // gbArray(Define) defines;

    Package package;
    map_t rename_map;
    BindConfig *conf;
    // RenameOpt rename_opt;

    b32 source_order;

    int proc_link_padding;
    int proc_name_padding;

    int var_link_padding;
    int var_name_padding;

    gbAllocator allocator;
} Printer;

Printer make_printer(Resolver resolver);
void print_package(Printer p);

#endif /* ifndef C_PARSER_PRINT_H_ */
