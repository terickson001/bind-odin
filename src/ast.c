#include "ast.h"
#include "util.h"

TypeInfo get_type_info(Node *type, gbAllocator a)
{
    TypeInfo ti = {0};
    for (;;)
    {
        switch (type->kind)
        {
        case NodeKind_PointerType:
            type = type->PointerType.type;
            ti.stars++;
            continue;
        case NodeKind_ArrayType:
            ti.is_array = true;
            type = type->ArrayType.type;
            continue;
        case NodeKind_ConstType:
            type = type->ConstType.type;
            ti.is_const = true;
        default: break;
        }
        break;
    }
    
    ti.base_type = type;
    return ti;
}

void __print_node(Node *node)
{
    if (!node)
        return;
    switch (node->kind)
    {
    case NodeKind_FunctionDecl:
        __print_node(node->FunctionDecl.ret_type);
        __print_node(node->FunctionDecl.name);
        gb_printf("(");
        __print_node(node->FunctionDecl.params);
        gb_printf(")");
        break;
    case NodeKind_Ident:
        gb_printf("%.*s", LIT(node->Ident.token.str));
        break;
    case NodeKind_FunctionType:
        __print_node(node->FunctionType.ret_type);
        gb_printf("(*)");
        gb_printf("(");
        __print_node(node->FunctionType.params);
        gb_printf(")");
        break;
    case NodeKind_IntegerType: {
        TypeInfo info = get_type_info(node, gb_heap_allocator());
        gb_printf("%.*s", LIT(integer_type_str(&info)));
        break;
    }
    case NodeKind_PointerType:
        __print_node(node->PointerType.type);
        gb_printf("*");
        break;
    case NodeKind_ArrayType:
        __print_node(node->ArrayType.type);
        gb_printf("[");
        __print_node(node->ArrayType.count);
        gb_printf("]");
        break;
    case NodeKind_StructType:
        gb_printf("struct ");
        __print_node(node->StructType.name);
        gb_printf("{");
        __print_node(node->StructType.fields);
        gb_printf("}");
        break;
    case NodeKind_UnionType:
        gb_printf("union ");
        __print_node(node->UnionType.name);
        gb_printf("{");
        __print_node(node->UnionType.fields);
        gb_printf("}");
        break;
    case NodeKind_EnumType:
        gb_printf("enum ");
        __print_node(node->EnumType.name);
        gb_printf("{");
        __print_node(node->EnumType.fields);
        gb_printf("}");
        break;
    case NodeKind_VarDeclList:
        for (int i = 0; i < gb_array_count(node->VarDeclList.list); i++)
        {
            __print_node(node->VarDeclList.list[i]);
            switch (node->VarDeclList.kind)
            {
            case VarDecl_Field:
                gb_printf(";");
                break;
                
            case VarDecl_Parameter:
            case VarDecl_NamelessParameter:
                if (gb_array_count(node->VarDeclList.list) != i+1)
                gb_printf(",");
                
            case VarDecl_Variable:
            default:
                break;
            }

        }
        break;
    case NodeKind_VarDecl:
        __print_node(node->VarDecl.type);
        __print_node(node->VarDecl.name);
        break;
    case NodeKind_EnumFieldList:
        for (int i = 0; i < gb_array_count(node->EnumFieldList.fields); i++)
        {
            __print_node(node->EnumFieldList.fields[i]);
            if (gb_array_count(node->EnumFieldList.fields) != i+1)
                gb_printf(",");
        }
        break;
    case NodeKind_EnumField:
        __print_node(node->EnumField.name);
        break;
    case NodeKind_VaArgs:
        gb_printf("...");
        return;
    case NodeKind_ConstType:
        gb_printf("const ");
        __print_node(node->ConstType.type);
        break;
    case NodeKind_Typedef:
        gb_printf("typedef ");
        __print_node(node->Typedef.var_list);
        break;

    case NodeKind_BasicLit:
        switch (node->BasicLit.token.kind)
        {
        case Token_String: gb_printf("\"%.*s\"", LIT(node->BasicLit.token.str)); break;
        case Token_Char:   gb_printf("'%.*s'",   LIT(node->BasicLit.token.str)); break;
        case Token_Wchar:  gb_printf("L'%.*s'",  LIT(node->BasicLit.token.str)); break;
        default:           gb_printf("%.*s",     LIT(node->BasicLit.token.str));
        }
        break;
    case NodeKind_UnaryExpr:
        gb_printf("%.*s ", LIT(node->UnaryExpr.op.str));
        __print_node(node->UnaryExpr.operand);
        break;
    case NodeKind_BinaryExpr:
        __print_node(node->BinaryExpr.left);
        gb_printf(" %.*s ", LIT(node->BinaryExpr.op.str));
        __print_node(node->BinaryExpr.right);
        break;
    case NodeKind_TernaryExpr:
        __print_node(node->TernaryExpr.cond);
        gb_printf(" ? ");
        __print_node(node->TernaryExpr.then);
        gb_printf(" : ");
        __print_node(node->TernaryExpr.els_);
        break;
    case NodeKind_IncDecExpr:
        __print_node(node->IncDecExpr.expr);
        gb_printf("%.*s", LIT(node->IncDecExpr.op.str));
        break;
    case NodeKind_CallExpr:
        __print_node(node->CallExpr.func);
        gb_printf("()");
        break;
    case NodeKind_ParenExpr:
        gb_printf("(");
        __print_node(node->ParenExpr.expr);
        gb_printf(")");
        break;
    case NodeKind_IndexExpr:
        __print_node(node->IndexExpr.expr);
        gb_printf("[");
        __print_node(node->IndexExpr.index);
        gb_printf("]");
        break;
    case NodeKind_SelectorExpr:
        __print_node(node->SelectorExpr.expr);
        gb_printf("%.*s", LIT(node->SelectorExpr.token.str));
        __print_node(node->SelectorExpr.selector);
        break;
    default: return;
    }
    gb_printf(" ");
}

void print_ast_node(Node *node)
{
    __print_node(node);
    gb_printf(";\n");
}

void print_ast_file(Ast_File file)
{
    for (int i = 0; i < gb_array_count(file.all_nodes); i++)
        print_ast_node(file.all_nodes[i]);
}