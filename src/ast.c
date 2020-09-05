#include "ast.h"
#include "util.h"

TypeInfo get_type_info(Node *type)
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
            continue;
        case NodeKind_BitfieldType:
            //type = type->BitfieldType.type;
            ti.is_bitfield = true;
            break;
        default: break;
        }
        break;
    }

    ti.base_type = type;
    return ti;
}

void __print_node(Node *node, int depth)
{
    if (!node)
        return;
    switch (node->kind)
    {
    case NodeKind_FunctionDecl:
        __print_node(node->FunctionDecl.type->FunctionType.ret_type, depth+1);
        __print_node(node->FunctionDecl.name, depth+1);
        gb_printf("(");
        __print_node(node->FunctionDecl.type->FunctionType.params, depth+1);
        gb_printf(")");
        break;
    case NodeKind_Ident:
        gb_printf("%.*s", LIT(node->Ident.token.str));
        break;
    case NodeKind_FunctionType:
        __print_node(node->FunctionType.ret_type, depth+1);
        gb_printf("(");
        __print_node(node->FunctionType.params, depth+1);
        gb_printf(")");
        break;
    case NodeKind_IntegerType: {
        gb_printf("%.*s", LIT(integer_type_str(node)));
        break;
    }
    case NodeKind_PointerType:
        __print_node(node->PointerType.type, depth+1);
        gb_printf("*");
        break;
    case NodeKind_ArrayType:
        __print_node(node->ArrayType.type, depth+1);
        gb_printf("[");
        __print_node(node->ArrayType.count, depth+1);
        gb_printf("]");
        break;
    case NodeKind_StructType:
        gb_printf("struct ");
        __print_node(node->StructType.name, depth+1);
        gb_printf("{");
        __print_node(node->StructType.fields, depth+1);
        gb_printf("}");
        break;
    case NodeKind_UnionType:
        gb_printf("union ");
        __print_node(node->UnionType.name, depth+1);
        gb_printf("{");
        __print_node(node->UnionType.fields, depth+1);
        gb_printf("}");
        break;
    case NodeKind_EnumType:
        gb_printf("enum ");
        __print_node(node->EnumType.name, depth+1);
        gb_printf("{");
        __print_node(node->EnumType.fields, depth+1);
        gb_printf("}");
        break;
    case NodeKind_VarDeclList:
        for (int i = 0; i < gb_array_count(node->VarDeclList.list); i++)
        {
            __print_node(node->VarDeclList.list[i], depth+1);
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
        __print_node(node->VarDecl.type, depth+1);
        __print_node(node->VarDecl.name, depth+1);
        break;
    case NodeKind_EnumFieldList:
        for (int i = 0; i < gb_array_count(node->EnumFieldList.fields); i++)
        {
            __print_node(node->EnumFieldList.fields[i], depth+1);
            if (gb_array_count(node->EnumFieldList.fields) != i+1)
                gb_printf(",");
        }
        break;
    case NodeKind_EnumField:
        __print_node(node->EnumField.name, depth+1);
        break;
    case NodeKind_VaArgs:
        gb_printf("...");
        return;
    case NodeKind_ConstType:
        gb_printf("const ");
        __print_node(node->ConstType.type, depth+1);
        break;
    case NodeKind_Typedef:
        gb_printf("typedef ");
        __print_node(node->Typedef.var_list, depth+1);
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
        __print_node(node->UnaryExpr.operand, depth+1);
        break;
    case NodeKind_BinaryExpr:
        __print_node(node->BinaryExpr.left, depth+1);
        gb_printf(" %.*s ", LIT(node->BinaryExpr.op.str));
        __print_node(node->BinaryExpr.right, depth+1);
        break;
    case NodeKind_TernaryExpr:
        __print_node(node->TernaryExpr.cond, depth+1);
        gb_printf(" ? ");
        __print_node(node->TernaryExpr.then, depth+1);
        gb_printf(" : ");
        __print_node(node->TernaryExpr.els_, depth+1);
        break;
    case NodeKind_IncDecExpr:
        __print_node(node->IncDecExpr.expr, depth+1);
        gb_printf("%.*s", LIT(node->IncDecExpr.op.str));
        break;
    case NodeKind_CallExpr:
        __print_node(node->CallExpr.func, depth+1);
        gb_printf("()");
        break;
    case NodeKind_ParenExpr:
        gb_printf("(");
        __print_node(node->ParenExpr.expr, depth+1);
        gb_printf(")");
        break;
    case NodeKind_IndexExpr:
        __print_node(node->IndexExpr.expr, depth+1);
        gb_printf("[");
        __print_node(node->IndexExpr.index, depth+1);
        gb_printf("]");
        break;
    case NodeKind_SelectorExpr:
        __print_node(node->SelectorExpr.expr, depth+1);
        gb_printf("%.*s", LIT(node->SelectorExpr.token.str));
        __print_node(node->SelectorExpr.selector, depth+1);
        break;
    default: return;
    }
    gb_printf(" ");
}

void print_ast_node(Node *node)
{
    __print_node(node, 0);
    gb_printf(";\n");
}

void print_ast_file(Ast_File file)
{
    for (int i = 0; i < gb_array_count(file.all_nodes); i++)
        print_ast_node(file.all_nodes[i]);
}

Token *node_token(Node *node)
{
    switch (node->kind)
    {
    case NodeKind_Ident: return &node->Ident.token;
    case NodeKind_IntegerType:  return &node->IntegerType.specifiers[0];
    case NodeKind_FloatType:  return &node->FloatType.specifiers[0];
    case NodeKind_PointerType:  return &node->PointerType.token;
    case NodeKind_ConstType:  return &node->ConstType.token;
    case NodeKind_FunctionDecl: return node_token(node->FunctionDecl.type);
    case NodeKind_FunctionType: return node_token(node->FunctionType.ret_type);
    case NodeKind_ArrayType: return node_token(node->ArrayType.type);
    case NodeKind_StructType: return &node->StructType.token;
    case NodeKind_UnionType: return &node->UnionType.token;
    case NodeKind_EnumType: return &node->EnumType.token;
    case NodeKind_BitfieldType: return node_token(node->BitfieldType.type);
    case NodeKind_VarDeclList: return node_token(node->VarDeclList.list[0]);
    case NodeKind_VarDecl: return node_token(node->VarDecl.type);
    case NodeKind_EnumFieldList: return node_token(node->EnumFieldList.fields[0]);
    case NodeKind_EnumField: return node_token(node->EnumField.name);
    case NodeKind_VaArgs: return &node->VaArgs.token;
    case NodeKind_Typedef: return &node->Typedef.token;
    case NodeKind_BasicLit: return &node->BasicLit.token;
    case NodeKind_UnaryExpr: return &node->UnaryExpr.op;
    case NodeKind_BinaryExpr: return node_token(node->BinaryExpr.left);
    case NodeKind_TernaryExpr: return node_token(node->TernaryExpr.cond);
    case NodeKind_IncDecExpr: return node_token(node->IncDecExpr.expr);
    case NodeKind_CallExpr: return node_token(node->CallExpr.func);
    case NodeKind_ParenExpr: return &node->ParenExpr.open;
    case NodeKind_IndexExpr: return node_token(node->IndexExpr.expr);
    case NodeKind_SelectorExpr: return node_token(node->SelectorExpr.expr);
    case NodeKind_TypeCast: return &node->TypeCast.open;
    case NodeKind_ExprList: return node_token(node->ExprList.list[0]);
    case NodeKind_EmptyStmt: return &node->EmptyStmt.token;
    case NodeKind_ExprStmt: return node_token(node->ExprStmt.expr);
    case NodeKind_AssignStmt: return node_token(node->AssignStmt.lhs);
    case NodeKind_IfStmt: return &node->IfStmt.token;
    case NodeKind_ForStmt: return &node->ForStmt.token;
    case NodeKind_WhileStmt: return &node->WhileStmt.token;
    case NodeKind_ReturnStmt: return &node->ReturnStmt.token;
    case NodeKind_SwitchStmt: return &node->SwitchStmt.token;
    case NodeKind_CaseStmt: return &node->CaseStmt.token;
    case NodeKind_Define:   return node_token(node->Define.value);
    default: return 0;
    }
}
