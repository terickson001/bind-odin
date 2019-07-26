#ifndef C_PARSER_PARSER_H_
#define C_PARSER_PARSER_H_ 1

#include "gb/gb.h"

#include "types.h"
#include "tokenizer.h"
#include "ast.h"

gb_global const Node NODE_INVALID =
{
    .kind = NodeKind_Invalid,
    .index = 0
};

typedef struct Parser
{
    Token *start, *curr, *end;
    gbAllocator alloc;
    int node_index;

    Ast_File file;

    gbArray(Node *) arrays_to_free;
} Parser;

Node *make_node(Parser *parser, Node_Kind kind);
Node *node_invalid(Parser *parser);
Node *node_type(Parser *parser, Node *name, int stars, Expr *array, b32 is_const);
Node *node_variable(Parser *parser, Node *type, String name, String value);
Node *node_identifier(Parser *parser, Token ident_token);
Node *node_function(Parser *parser, Node *ret_type, String name, gbArray(Node) params);
Node *node_record(Parser *parser, Record_Kind kind, String name, gbArray(Node) fields);
Node *node_typedef(Parser *parser, Node *type, String name);
Node *node_expression(Parser *parser, gbArray(Token) tokens);

Parser make_parser(gbArray(Token) tokens);
void destroy_parser(Parser *parser);
void parser_advance_token(Parser *parser);
b32 parser_try(Node *(*parse_func)(Parser *), Parser *parser, Node **node_ret);

Node *parse_type(Parser *parser);
Node *_parse_variable(Parser *parser, Node *type, b32 first);
gbArray(Node) parse_variable(Parser *parser);
b32 parse_var_list(Parser *parser, gbArray(Node) *vars, TokenKind begin, TokenKind end, TokenKind delim);
Node *parse_record(Parser *parser);
Node *parse_function(Parser *parser);
Node *parse_typedef(Parser *parser);
Node *parse_type(Parser *parser);

void parse_file(Parser *parser);

#endif /* ifndef C_PARSER_PARSER_H_ */
