#ifndef C_PARSER_PRINT_H_
#define C_PARSER_PRINT_H_ 1

#include "ast.h"
#include "strings.h"
#include "util.h"
#include "resolve.h"
#include "config.h"

typedef struct Printer
{
     
     Ast_File file;
     gbFile *out_file;
     Package package;
     
     map_t rename_map;
     BindConfig *conf;
     
     map_t wrap_rename_map;
     WrapConfig *wrap_conf;
     
     b32 source_order;
     
     int proc_link_padding;
     int proc_name_padding;
     
     int var_link_padding;
     int var_name_padding;
     
     gbAllocator allocator;
} Printer;

Printer make_printer(Resolver resolver);
void print_package(Printer p);
void print_indent(Printer p, int indent);
void print_ident(Printer p, Node *node, int indent);
void print_basic_lit(Printer p, Node *node, int indent);
void print_inc_dec_expr(Printer p, Node *node, int indent);
void print_call_expr(Printer p, Node *node, int indent);
void print_index_expr(Printer p, Node *node, int indent);
void print_paren_expr(Printer p, Node *node, int indent);
void print_unary_expr(Printer p, Node *node, int indent);
void print_binary_expr(Printer p, Node *node, int indent);
void print_ternary_expr(Printer p, Node *node, int indent);
void print_expr(Printer p, Node *node, int indent);
void print_expr_list(Printer p, Node *node, int indent);
void print_array_type(Printer p, Node *node, int indent);
void print_function_type(Printer p, Node *node, int indent);
void print_base_type(Printer p, Node *node, int indent);
void print_type(Printer p, Node *node, int indent);
void print_va_args(Printer p, Node *node, int indent);
void print_variable(Printer p, Node *node, int indent, b32 top_level, int name_padding);
void print_enum_field(Printer p, Node *node, int indent);
void print_enum(Printer p, Node *node, int indent, b32 top_level, b32 indent_first);
void print_record(Printer p, Node *node, int indent, b32 top_level, b32 indent_first);
void print_function_parameters(Printer p, Node *node, int indent);
void print_function(Printer p, Node *node, int indent);
void print_typedef(Printer p, Node *node, int indent);
void print_string(Printer p, String str, int indent);
void print_node(Printer p, Node *node, int indent, b32 top_level, b32 indent_first);
void print_file(Printer p);

#endif /* ifndef C_PARSER_PRINT_H_ */
