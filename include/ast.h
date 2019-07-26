#ifndef _C_PARSER_AST_H_
#define _C_PARSER_AST_H_

#include "gb/gb.h"
#include "tokenizer.h"
#include "string.h"

typedef struct Define Define;
typedef struct Node Node;

typedef enum VarDeclKind
{
    VarDecl_Variable,
    VarDecl_Field,
    VarDecl_Parameter,
    VarDecl_NamelessParameter,
    VarDecl_VaArgs,
    VarDecl_AnonRecord,
} VarDeclKind;
    
#define NODE_KINDS                                                      \
    NODE_KIND(Ident, "identifier", struct {                             \
            Token token;                                                \
            String ident;                                               \
    })                                                                  \
    NODE_KIND(Typedef, "typedef", struct {                              \
            Token token;                                                \
            Node *var_list;                                             \
    })                                                                  \
    NODE_KIND(BasicLit, "basic literal", struct {                       \
            Token token;                                                \
    })                                                                  \
    NODE_KIND(CompoundLit, "compound literal", struct {                 \
            Node *fields;                                               \
            Token open, close;                                          \
    })                                                                  \
    NODE_KIND(Attribute, "attribute", struct {                          \
            Node *name;                                                 \
            Node *ident;                                                \
            Node *args;                                                 \
    })                                                                  \
    NODE_KIND(AttrList, "attribute list", struct {                      \
            gbArray(Node *) list;                                       \
    })                                                                  \
    NODE_KIND(Define, "define directive", struct {                      \
            String name;                                                \
            Node *value;                                                \
    })                                                                  \
NODE_KIND(_ExprBegin, "", bool)                                         \
    NODE_KIND(InvalidExpr,  "invalid expression", struct {              \
            Token start;                                                \
            Token end;                                                  \
    })                                                                  \
    NODE_KIND(UnaryExpr, "unary expression", struct {                   \
            Token op;                                                   \
            Node *operand;                                              \
    })                                                                  \
    NODE_KIND(BinaryExpr, "binary expression", struct {                 \
            Token op;                                                   \
            Node *left, *right;                                         \
    })                                                                  \
    NODE_KIND(TernaryExpr, "ternary expression", struct {               \
            Node *cond;                                                 \
            Node *then, *els_;                                          \
    })                                                                  \
    NODE_KIND(ParenExpr, "parentheses expression", struct {             \
            Node *expr;                                                 \
            Token open, close;                                          \
    })                                                                  \
    NODE_KIND(SelectorExpr, "selector expression", struct {             \
            Token token;                                                \
            Node *expr, *selector;                                      \
    })                                                                  \
    NODE_KIND(IndexExpr, "index expression", struct {                   \
            Node *expr, *index;                                         \
            Token open, close;                                          \
    })                                                                  \
    NODE_KIND(CallExpr, "call expression", struct {                     \
            Node *func;                                                 \
            Node *args;                                                 \
            Token open, close;                                          \
    })                                                                  \
    NODE_KIND(TypeCast, "type cast", struct {                           \
            Node *type;                                                 \
            Node *expr;                                                 \
            Token open, close;                                          \
    })                                                                  \
    NODE_KIND(IncDecExpr, "increment/decrement expression", struct {    \
            Node *expr;                                                 \
            Token op;                                                   \
    })                                                                  \
    NODE_KIND(ExprList, "expression list", struct {                     \
            gbArray(Node *) list;                                       \
    })                                                                  \
NODE_KIND(_ExprEnd, "", b32)                                            \
NODE_KIND(_StmtBegin, "", b32)                                          \
    NODE_KIND(EmptyStmt, "empty statement", struct {                    \
            Token token;                                                \
    })                                                                  \
    NODE_KIND(ExprStmt, "expression statement", struct {                \
            Node *expr;                                                 \
    })                                                                  \
    NODE_KIND(AssignStmt, "assignment statement", struct {              \
            Token token;                                                \
            Node *lhs, *rhs;                                            \
    })                                                                  \
    NODE_KIND(CompoundStmt, "compund statement", struct {               \
            Token open, close;                                          \
            gbArray(Node *) stmts;                                      \
    })                                                                  \
    NODE_KIND(IfStmt, "if statement", struct {                          \
            Token token;                                                \
            Node *cond;                                                 \
            Node *body;                                                 \
            Node *els_;                                                 \
    })                                                                  \
    NODE_KIND(ForStmt, "for statement", struct {                        \
            Token token;                                                \
            Node *init;                                                 \
            Node *cond;                                                 \
            Node *post;                                                 \
            Node *body;                                                 \
    })                                                                  \
    NODE_KIND(WhileStmt, "while statement", struct {                    \
            Token token;                                                \
            Node *cond;                                                 \
            Node *body;                                                 \
            b32 on_exit;                                                \
    })                                                                  \
    NODE_KIND(ReturnStmt, "return statement", struct {                  \
            Token token;                                                \
            Node *expr;                                                 \
    })                                                                  \
    NODE_KIND(SwitchStmt, "switch statement", struct {                  \
            Token token;                                                \
            Node *expr;                                                 \
            Node *body;                                                 \
    })                                                                  \
    NODE_KIND(CaseStmt, "case statement", struct {                      \
            Token token;                                                \
            Node *value;                                                \
            gbArray(Node *) stmts;                                      \
    })                                                                  \
    NODE_KIND(BranchStmt, "branch statement", struct {                  \
            Token token;                                                \
    })                                                                  \
NODE_KIND(_StmtEnd, "", b32)                                            \
NODE_KIND(_DeclBegin, "", b32)                                          \
    NODE_KIND(VarDecl, "variable declaration", struct {                 \
            Node *type;                                                 \
            Node *name;                                                 \
            VarDeclKind kind;                                           \
    })                                                                  \
    NODE_KIND(VarDeclList, "variable declaration list", struct {        \
        gbArray(Node *) list;                                           \
        VarDeclKind kind;                                               \
    })                                                                  \
    NODE_KIND(EnumField, "enum field", struct {                         \
        Node *name, *value;                                             \
    })                                                                  \
    NODE_KIND(EnumFieldList, "enum field list", struct {                \
        gbArray(Node *) fields;                                         \
    })                                                                  \
    NODE_KIND(FunctionDecl, "function declaration", struct {            \
            Node *ret_type;                                             \
            Node *name;                                                 \
            Node *params;                                               \
            Node *body;                                                 \
            b32 inlined;                                                \
    })                                                                  \
NODE_KIND(_DeclEnd, "", b32)                                            \
    NODE_KIND(VaArgs, "variadic argument", struct {                     \
            Token token;                                                \
    })                                                                  \
NODE_KIND(_TypeBegin, "", b32)                                          \
    NODE_KIND(IntegerType, "integer type", struct {                     \
            gbArray(Token) specifiers;                                  \
    })                                                                  \
    NODE_KIND(PointerType, "pointer type", struct {                     \
            Token token;                                                \
            Node *type;                                                 \
    })                                                                  \
    NODE_KIND(ArrayType, "array type", struct {                         \
            Node *type;                                                 \
            Node *count;                                                \
            Token open, close;                                          \
    })                                                                  \
    NODE_KIND(ConstType, "constant type", struct {                      \
            Token token;                                                \
            Node *type;                                                 \
    })                                                                  \
    NODE_KIND(StructType, "struct type", struct {                       \
            Token token;                                                \
            Node *name;                                                 \
            Node *fields;                                               \
    })                                                                  \
    NODE_KIND(UnionType, "union type", struct {                         \
            Token token;                                                \
            Node *name;                                                 \
            Node *fields;                                               \
    })                                                                  \
    NODE_KIND(EnumType, "enum type", struct {                           \
            Token token;                                                \
            Node *name;                                                 \
            Node *fields;                                               \
    })                                                                  \
    NODE_KIND(FunctionType, "function type", struct {                   \
            Node *ret_type;                                             \
            Node *params;                                               \
            Token open, close, pointer;                                 \
    })                                                                  \
NODE_KIND(_TypeEnd, "", b32)


typedef enum NodeKind {
    NodeKind_Invalid,
#define NODE_KIND(KIND_, DESC_, ...) NodeKind_##KIND_,
    NODE_KINDS
#undef NODE_KIND
    NodeKind_Count,
} NodeKind;

gb_global String const node_strings[] =
{
    {"invalid node", gb_size_of("invalid node")-1},
#define NODE_KIND(KIND_, DESC_, ...) {DESC_, gb_size_of(DESC_)-1},
    NODE_KINDS
#undef NODE_KIND
};

#define NODE_KIND(KIND_, DESC_, ...) typedef __VA_ARGS__ Node_##KIND_;
NODE_KINDS
#undef NODE_KIND

typedef struct Node {
    NodeKind kind;

    // This info is all for the printer
    i32 index;
    b32 no_print;
    b32 is_opaque;
    
    union {
#define NODE_KIND(KIND_, DESC_, ...) Node_##KIND_ KIND_;
        NODE_KINDS
#undef NODE_KIND
    };
} Node;

typedef struct Ast_File
{
    char *filename;

    gbArray(Node *) all_nodes;

    gbArray(Node *) tpdefs;
    gbArray(Node *) records;
    gbArray(Node *) functions;
    gbArray(Node *) variables;
    
    gbArray(Define) raw_defines;
    gbArray(Node *) defines;
} Ast_File;

typedef struct TypeInfo
{
    Node *base_type;
    int stars;
    b32 is_const;
    b32 is_array;
} TypeInfo;
TypeInfo get_type_info(Node *type, gbAllocator a);

void print_ast_node(Node *node);
void print_ast_file(Ast_File file);
    
#endif
