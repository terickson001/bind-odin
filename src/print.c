#include "print.h"

#include "types.h"
#include "strings.h"
#include "util.h"
#include "error.h"
#include "tokenizer.h"

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

void print_indent(Printer p, int indent)
{
    if (!indent) return;
    char *spaces = repeat_char(' ', indent*4, p.allocator);
    gb_fprintf(p.out_file, "%s", spaces);
    gb_free(p.allocator, spaces);
}

Printer make_printer(Resolver resolver)
{
    Printer printer = {0};
    printer.allocator = gb_heap_allocator();
    printer.package = resolver.package;

    printer.rename_map = resolver.rename_map;
    printer.needs_opaque_def = resolver.needs_opaque_def;
    printer.conf = resolver.conf;

    init_rename_map(printer.rename_map, printer.allocator);

    return printer;
}

void print_ident(Printer p, Node *node, int indent)
{
    print_string(p, node->Ident.ident, indent);
}

char *odinize_number(String num, gbAllocator a)
{
    char *prefix = 0;
    if (csubstring_cmp(num, "0x") == 0 || csubstring_cmp(num, "0X") == 0)
    {
        num.start += 2; num.len -= 2;
        prefix = "0x";
    }
    else if (num.start[0] == '0' && num.len > 1 && gb_char_is_digit(num.start[1]))
    {
        num.start += 1; num.len -= 1;
        prefix = "0o";
    }

    int i = 0;
    for (;i < num.len; i++)
    {
        switch (num.start[i])
        {
            case 'f':
            case 'F':
            if (gb_strcmp(prefix, "0x") == 0)
                continue;
            case 'L':
            case 'l':
            case 'u':
            case 'U':
            case 'i':
            break;
            default: continue;
        }
        num.len = i;
        break;
    }

    int len = (prefix?2:0) + num.len;
    char *ret = gb_alloc(a, len+1);
    gb_snprintf(ret, len, "%s%.*s", prefix, LIT(num));

    return ret;
}

void print_basic_lit(Printer p, Node *node, int indent)
{
    switch (node->BasicLit.token.kind)
    {
        case Token_String:
        gb_fprintf(p.out_file, "\"%.*s\"", LIT(node->BasicLit.token.str));
        break;

        case Token_Char:
        case Token_Wchar:
        gb_fprintf(p.out_file, "'%.*s'", LIT(node->BasicLit.token.str));
        break;

        case Token_Float:
        case Token_Integer: {
            char *odinized = odinize_number(node->BasicLit.token.str, p.allocator);
            gb_fprintf(p.out_file, "%s", odinized);
            gb_free(p.allocator, odinized);
        } break;

        default:
        gb_fprintf(p.out_file, "%.*s", LIT(node->BasicLit.token.str));
        break;
    }
}
void print_inc_dec_expr(Printer p, Node *node, int indent)
{
    print_expr(p, node->IncDecExpr.expr, indent);
    gb_fprintf(p.out_file, "%.*s", LIT(node->IncDecExpr.op.str));
}
void print_call_expr(Printer p, Node *node, int indent)
{
    print_expr(p, node->CallExpr.func, indent);
    gb_fprintf(p.out_file, "(");
    print_expr_list(p, node->CallExpr.args, 0);
    gb_fprintf(p.out_file, ")");
}
void print_index_expr(Printer p, Node *node, int indent)
{
    print_expr(p, node->IndexExpr.expr, indent);
    gb_fprintf(p.out_file, "[");
    print_expr(p, node->IndexExpr.index, 0);
    gb_fprintf(p.out_file, "]");
}
void print_paren_expr(Printer p, Node *node, int indent)
{
    gb_fprintf(p.out_file, "(");
    print_expr(p, node->ParenExpr.expr, 0);
    gb_fprintf(p.out_file, ")");
}

void print_unary_expr(Printer p, Node *node, int indent)
{
    if (node->UnaryExpr.op.kind == Token_Mul)
    {
        print_expr(p, node->UnaryExpr.operand, indent);
        gb_fprintf(p.out_file, "^");
    }
    else if (node->UnaryExpr.op.kind == Token_sizeof)
    {
        gb_fprintf(p.out_file, "size_of");
        print_expr(p, node->UnaryExpr.operand, indent);
    }
    else
    {
        gb_fprintf(p.out_file, "%.*s", LIT(node->UnaryExpr.op.str));
        print_expr(p, node->UnaryExpr.operand, 0);
    }
}
void print_binary_expr(Printer p, Node *node, int indent)
{
    print_expr(p, node->BinaryExpr.left, indent);
    if (node->BinaryExpr.op.kind != Token_Xor)
        gb_fprintf(p.out_file, " %.*s ", LIT(node->BinaryExpr.op.str));
    else
        gb_fprintf(p.out_file, " ~ ");
    print_expr(p, node->BinaryExpr.right, 0);
}
void print_ternary_expr(Printer p, Node *node, int indent)
{
    print_expr(p, node->TernaryExpr.cond, indent);
    gb_fprintf(p.out_file, " ? ");
    print_expr(p, node->TernaryExpr.then, indent);
    gb_fprintf(p.out_file, " : ");
    print_expr(p, node->TernaryExpr.els_, indent);
}
void print_cast_expr(Printer p, Node *node, int indent)
{
    b32 add_parens = node->TypeCast.expr->kind != NodeKind_ParenExpr;
    gb_fprintf(p.out_file, "(");
    print_type(p, node->TypeCast.type, 0);
    gb_fprintf(p.out_file, ")");
    if (add_parens) gb_fprintf(p.out_file, "(");
    print_expr(p, node->TypeCast.expr, 0);
    if (add_parens) gb_fprintf(p.out_file, ")");
}
void print_expr(Printer p, Node *node, int indent)
{
    switch (node->kind)
    {
        case NodeKind_TernaryExpr: print_ternary_expr(p, node, indent); break;
        case NodeKind_BinaryExpr:  print_binary_expr (p, node, indent); break;
        case NodeKind_TypeCast:    print_cast_expr   (p, node, indent); break;
        case NodeKind_UnaryExpr:   print_unary_expr  (p, node, indent); break;
        case NodeKind_ParenExpr:   print_paren_expr  (p, node, indent); break;
        case NodeKind_IndexExpr:   print_index_expr  (p, node, indent); break;
        case NodeKind_CallExpr:    print_call_expr   (p, node, indent); break;
        case NodeKind_IncDecExpr:  print_inc_dec_expr(p, node, indent); break;
        case NodeKind_BasicLit:    print_basic_lit   (p, node, indent); break;
        case NodeKind_Ident:       print_ident       (p, node, indent); break;
        case NodeKind_IntegerType:
        case NodeKind_FloatType:
        case NodeKind_StructType:
        case NodeKind_EnumType:
        case NodeKind_UnionType:   print_type        (p, node, indent); break;
        default: break;// gb_printf_err("Invalid expression node: %.*s\n", LIT(node_strings[node->kind]));
    }
}

void print_expr_list(Printer p, Node *node, int indent)
{
    if (!node) return;
    for (int i = 0; i < gb_array_count(node->ExprList.list); i++)
    {
        print_expr(p, node->ExprList.list[i], indent);
        if (gb_array_count(node->ExprList.list) > i+1)
            gb_fprintf(p.out_file, ", ");
        indent = 0;
    }
}

void print_array_type(Printer p, Node *node, int indent)
{
    gb_fprintf(p.out_file, "[");
    print_expr(p, node->ArrayType.count, 0);
    gb_fprintf(p.out_file, "]");
    print_indent(p, indent);
    print_type(p, node->ArrayType.type, 0);
}

void print_calling_convention(Printer p, Token token)
{
    switch (token.kind)
    {
    case Token_stdcall:
        gb_fprintf(p.out_file, " \"stdcall\" ");
        break;
    case Token_fastcall:
        gb_fprintf(p.out_file, " \"fastcall\" ");
        break;
    }
}

void print_function_type(Printer p, Node *node, int indent)
{
    print_indent(p, indent);

    b32 ret_void =
        node->FunctionType.ret_type->kind == NodeKind_Ident
        && cstring_cmp(node->FunctionType.ret_type->Ident.token.str, "void") == 0;

    gb_fprintf(p.out_file, "%cproc", ret_void ? 0 : '(');
    print_calling_convention(p, node->FunctionType.calling_convention);
    gb_fprintf(p.out_file, "(");
    if (node->FunctionType.params)
        print_function_parameters(p, node->FunctionType.params, 0);
    gb_fprintf(p.out_file, ")");

    if (!ret_void)
    {
        gb_fprintf(p.out_file, " -> ");
        print_type(p, node->FunctionType.ret_type, 0);
        gb_fprintf(p.out_file, ")");
    }
}

b32 top_level_type = false;
void print_base_type(Printer p, Node *node, int indent)
{
    String name = convert_type(node, p.rename_map, p.conf, p.allocator);

    switch (node->kind)
    {
        case NodeKind_StructType:
        case NodeKind_UnionType:
        case NodeKind_EnumType:
        if (node->StructType.fields)
        {
            print_node(p, node, indent, false, false);
            return;
        } break;
        default: break;
    }

    print_string(p, name, 0);
}

void print_type(Printer p, Node *node, int indent)
{
    switch (node->kind)
    {
        case NodeKind_ArrayType:    print_array_type   (p, node, 0); break;
        case NodeKind_FunctionType: print_function_type(p, node, 0); break;

        case NodeKind_ConstType:    print_type(p, node->ConstType.type, 0); break;

        case NodeKind_PointerType: { // Lots of special cases for pointer types
            Node *child = node->PointerType.type;
            while (child->kind == NodeKind_ConstType)
                child = child->ConstType.type;

            switch (child->kind)
            {
                case NodeKind_FunctionType: print_function_type(p, child, 0); return;
                case NodeKind_Ident: {
                    String name = child->Ident.token.str;
                    if (cstring_cmp(name, "void") == 0)
                    {
                        gb_fprintf(p.out_file, "rawptr");
                        return;
                    }
                } break;
                case NodeKind_IntegerType: {
                    if (!p.conf->use_cstring)
                        break;
                    String str = integer_type_str(child);
                    if (cstring_cmp(str, "u8") == 0)
                    {
                        gb_fprintf(p.out_file, "cstring");
                        return;
                    }
                } break;
            }
            gb_fprintf(p.out_file, "^");
            print_type(p, child, 0);
        } break;

        case NodeKind_BitfieldType: print_expr(p, node->BitfieldType.size, 0); break;

        case NodeKind_StructType:
        case NodeKind_UnionType:
        case NodeKind_EnumType:
        case NodeKind_IntegerType:
        case NodeKind_FloatType:
        case NodeKind_Ident:
        print_base_type(p, node, indent);
        break;

        case NodeKind_VaArgs:
        gb_fprintf(p.out_file, "..any");
        break;
        default:
        gb_printf_err("Invalid node as type: '%.*s'\n", LIT(node_strings[node->kind]));
        break;
    }
}

void print_va_args(Printer p, Node *node, int indent)
{
    gb_fprintf(p.out_file, "..any");
}

void print_variable(Printer p, Node *node, int indent, b32 top_level, int name_padding)
{
    print_indent(p, indent);

    TypeInfo type  = get_type_info(node->VarDecl.type);
    switch (node->VarDecl.kind)
    {
        case VarDecl_Variable: {
            if (!top_level) break;
            String name    = node->VarDecl.name->Ident.token.str;
            String renamed = rename_ident(name, RENAME_VAR, true, p.rename_map, p.conf, p.allocator);

            int name_padding = p.var_name_padding;
            if (p.conf->var_case || (p.conf->var_prefix.len && !has_prefix(name, p.conf->var_prefix)))
            {
                if (!p.conf->var_case)
                    name_padding -= 16 + p.var_link_padding;
                char *link_name_padding = repeat_char(' ', p.var_link_padding - name.len, p.allocator);
                gb_fprintf(p.out_file, "@(link_name=\"%.*s\")%s ",
                           LIT(name), link_name_padding);
                gb_free(p.allocator, link_name_padding);
            }

            char *var_name_padding  = repeat_char(' ', name_padding - renamed.len, p.allocator);
            gb_fprintf(p.out_file, "%.*s%s : ",
                       LIT(renamed), var_name_padding);
            gb_free(p.allocator, var_name_padding);
        } break;

        case VarDecl_Field: {
            String name    = node->VarDecl.name->Ident.token.str;
            String renamed = rename_ident(name, RENAME_VAR, false, p.rename_map, p.conf, p.allocator);
            char *var_name_padding = 0;
            switch (type.base_type->kind)
            {
                // NOTE: This works because all these kinds have the same layout
                case NodeKind_StructType:
                case NodeKind_UnionType:
                case NodeKind_EnumType:
                if (type.base_type->StructType.fields) break; // Don't add padding for record definitions

                default:
                var_name_padding = repeat_char(' ', name_padding - renamed.len, p.allocator);
                break;
            }
            gb_fprintf(p.out_file, "%.*s%s : ", LIT(renamed), var_name_padding);
            if (var_name_padding) gb_free(p.allocator, var_name_padding);
        } break;


        case VarDecl_Parameter: {
            String name    = node->VarDecl.name->Ident.token.str;
            String renamed = rename_ident(name, RENAME_VAR, false, p.rename_map, p.conf, p.allocator);
            gb_fprintf(p.out_file, "%.*s : ", LIT(renamed));
        } break;

        case VarDecl_VaArgs:
        case VarDecl_NamelessParameter:
        case VarDecl_AnonBitfield:
        if (node->VarDecl.type->kind == NodeKind_BitfieldType)
                gb_fprintf(p.out_file, "_ : ");
        break;

        case VarDecl_AnonRecord:
        gb_fprintf(p.out_file, "using _ : ");
        break;
    }

    print_type(p, node->VarDecl.type, indent);

    // TODO(@Completeness): Implement variable values
    /*
    if (node->variable.value.start)
        gb_fprintf(printer.out_file, " %c %.*s",
        node->variable.type->type.is_const?':':'=', LIT(node->variable.value));
    */
}

// TODO(@Robustness): Consolidate Struct/Union/Enum into a NodeKind (RecordType)
//                    Requires consolidation of EnumField and VarDecl
void print_enum_field(Printer p, Node *node, int indent)
{
    print_indent(p, indent);

    String renamed = rename_ident(node->EnumField.name->Ident.token.str, RENAME_TYPE, false, p.rename_map, p.conf, p.allocator);
    gb_fprintf(p.out_file, "%.*s", LIT(renamed));
    if (node->EnumField.value)
    {
        gb_fprintf(p.out_file, " = ");
        print_expr(p, node->EnumField.value, indent);
    }
}

void _print_enum_field(Printer p, Node *node, Node *prev, int indent)
{
    print_indent(p, indent);

    String renamed = rename_ident(node->EnumField.name->Ident.token.str, RENAME_TYPE, false, p.rename_map, p.conf, p.allocator);
    gb_fprintf(p.out_file, "%.*s :: ", LIT(renamed));
    if (node->EnumField.value)
    {
        print_expr(p, node->EnumField.value, indent);
    }
    else if (prev)
    {
        String prev_renamed = rename_ident(prev->EnumField.name->Ident.token.str, RENAME_TYPE, false, p.rename_map, p.conf, p.allocator);
        gb_fprintf(p.out_file, "%.*s", LIT(prev_renamed));
        gb_fprintf(p.out_file, " + 1");
    }
    else
    {
        gb_fprintf(p.out_file, "0");
    }
}

void print_enum(Printer p, Node *node, int indent, b32 top_level, b32 indent_first)
{
    if (indent_first)
        print_indent(p, indent);

    if (node->EnumType.name)
    {
        String renamed = rename_ident(node->EnumType.name->Ident.token.str, RENAME_TYPE, true, p.rename_map, p.conf, p.allocator);
        if (top_level && node->EnumType.fields)
        {
            gb_fprintf(p.out_file, "/* %.*s :: enum { */\n%.*s :: _c.int;\n", LIT(renamed), LIT(renamed));
        }
        else if (!node->EnumType.fields)
        {
            gb_fprintf(p.out_file, "%.*s", LIT(renamed));
            return;
        }
    }
    else if (top_level)
    {
        gb_fprintf(p.out_file, "/* using _ :: enum  { */\n");
    }

    if (node->EnumType.fields)
    {
        gbArray(Node *) fields = node->EnumType.fields->EnumFieldList.fields;

        for (int i = 0; i < gb_array_count(fields); i++)
        {
            _print_enum_field(p, fields[i], i>0?fields[i-1]:0, indent+1);
            gb_fprintf(p.out_file, ";\n");
        }
    }

    gb_fprintf(p.out_file, "/* } */\n");
}
/*
void print_enum(Printer p, Node *node, int indent, b32 top_level, b32 indent_first)
{
    if (indent_first)
        print_indent(p, indent);

    if (node->EnumType.name)
    {
        String renamed = rename_ident(node->EnumType.name->Ident.token.str, RENAME_TYPE, true, p.rename_map, p.conf, p.allocator);
        if (top_level)
        {
            gb_fprintf(p.out_file, "using %.*s :: ", LIT(renamed));
        }
        else if (!node->EnumType.fields)
        {
            gb_fprintf(p.out_file, "%.*s", LIT(renamed));
            return;
        }
    }
    else if (top_level)
    {
        gb_fprintf(p.out_file, "using _ ::");
    }

    gb_fprintf(p.out_file, "enum {");
    if (node->EnumType.fields)
    {
        gbArray(Node *) fields = node->EnumType.fields->EnumFieldList.fields;

        gb_fprintf(p.out_file, "\n");
        for (int i = 0; i < gb_array_count(fields); i++)
        {
            print_enum_field(p, fields[i], indent+1);
            gb_fprintf(p.out_file, ",\n");
        }
    }

    print_indent(p, indent);
    gb_fprintf(p.out_file, "}");
}
*/
void print_record(Printer p, Node *node, int indent, b32 top_level, b32 indent_first)
{
    if (indent_first)
        print_indent(p, indent);

    if (node->StructType.name)
    {
        String renamed = rename_ident(node->StructType.name->Ident.token.str, RENAME_TYPE, true, p.rename_map, p.conf, p.allocator);
        if (top_level)
        {
            gb_fprintf(p.out_file, "%.*s :: ", LIT(renamed));
        }
        else if (!node->StructType.fields)
        {
            gb_fprintf(p.out_file, "%.*s", LIT(renamed));
            return;
        }
    }
    else if (top_level)
    {
        error(node->StructType.token, "No name given for record at file-scope");
    }


    switch (node->StructType.token.kind)
    {
        case Token_struct:
            if (node->StructType.has_bitfield && node->StructType.only_bitfield)
                gb_fprintf(p.out_file, "bit_field");
            else
                gb_fprintf(p.out_file, "struct");
            break;
        case Token_union:
            gb_fprintf(p.out_file, "struct #raw_union");
            break;
        default: break;
    }

    gb_fprintf(p.out_file, " {");
    if (node->StructType.fields)
    {
        gb_fprintf(p.out_file, "\n");

        gbArray(Node *) fields = node->StructType.fields->VarDeclList.list;
        int field_padding = 0;
        for (int i = 0; i < gb_array_count(fields); i++)
        {
            if (!fields[i]->VarDecl.name) continue;
            String rename_temp = rename_ident(fields[i]->VarDecl.name->Ident.token.str, RENAME_VAR, false, p.rename_map, p.conf, p.allocator);
            field_padding = gb_max(field_padding, rename_temp.len);
        }

        b32 in_bitfield = false;
        for (int i = 0; i < gb_array_count(fields); i++)
        {
            if (!node->StructType.only_bitfield)
            {
              if (!in_bitfield && fields[i]->VarDecl.type->kind == NodeKind_BitfieldType)
              {
                print_indent(p, indent+1);
                gb_fprintf(p.out_file, "using _ : bit_field {\n");
                in_bitfield = true;
                indent += 1;
              }
              else if (in_bitfield && fields[i]->VarDecl.type->kind != NodeKind_BitfieldType)
              {
                indent -= 1;
                print_indent(p, indent+1);
                gb_fprintf(p.out_file, "},\n");
                in_bitfield = false;
              }
            }
            print_variable(p, fields[i], indent+1, false, field_padding);
            gb_fprintf(p.out_file, ",\n");
        }
        if (in_bitfield)
        {
            indent -= 1;
            print_indent(p, indent+1);
            gb_fprintf(p.out_file, "},\n");
        }
    }

    print_indent(p, indent);
    gb_fprintf(p.out_file, "}");
}

void print_function_parameters(Printer p, Node *node, int indent)
{
    gbArray(Node *) params = node->VarDeclList.list;
    b32 use_param_names = true;

    // Check for `void` params
    if (gb_array_count(params) == 1
        && params[0]->VarDecl.kind == VarDecl_NamelessParameter
        && params[0]->VarDecl.type->kind == NodeKind_Ident
        && cstring_cmp(params[0]->VarDecl.type->Ident.token.str, "void") == 0)
        return;

    // Determine whether to use parameter names
    for (int i = 0; i < gb_array_count(params) && use_param_names; i++)
    {
        if (params[i]->VarDecl.kind == VarDecl_NamelessParameter)
            use_param_names = false;
    }

    for (int i = 0; i < gb_array_count(params); i++)
    {
        switch (params[i]->VarDecl.kind)
        {
            case VarDecl_Parameter:
            case VarDecl_NamelessParameter:
            if (use_param_names)
                print_variable(p, params[i], indent, false, 0);
            else
                print_type(p, params[i]->VarDecl.type, indent);
            break;
            case VarDecl_VaArgs:
            gb_fprintf(p.out_file, "#c_vararg %s..any", use_param_names?"__args : ":0);
            break;
            default: break;
        }
        if (i != gb_array_count(params) - 1)
            gb_fprintf(p.out_file, ", ");
    }
}

void print_function(Printer p, Node *node, int indent)
{
    print_indent(p, indent);
    String name    = node->FunctionDecl.name->Ident.token.str;
    String renamed = rename_ident(name, RENAME_PROC, true, p.rename_map, p.conf, p.allocator);

    int name_padding = p.proc_name_padding;
    if (p.conf->proc_case || (p.conf->proc_prefix.len && !has_prefix(name, p.conf->proc_prefix)))
    {
        if (!p.conf->proc_case)
            name_padding -= 16 + p.proc_link_padding;
        char *link_name_padding = repeat_char(' ', p.proc_link_padding - name.len,    p.allocator);
        gb_fprintf(p.out_file, "@(link_name=\"%.*s\")%s ",
                   LIT(name),    link_name_padding);
        gb_free(p.allocator, link_name_padding);
    }

    char *proc_name_padding = repeat_char(' ', name_padding - renamed.len, p.allocator);
    gb_fprintf(p.out_file, "%.*s%s :: ",
               LIT(renamed), proc_name_padding);
    gb_free(p.allocator, proc_name_padding);

    gb_fprintf(p.out_file, "proc");
    print_calling_convention(p, node->FunctionDecl.type->FunctionType.calling_convention);
    gb_fprintf(p.out_file, "(");

    if (node->FunctionDecl.type->FunctionType.params)
        print_function_parameters(p, node->FunctionDecl.type->FunctionType.params, 0);

    gb_fprintf(p.out_file, ")");

    TypeInfo info = get_type_info(node->FunctionDecl.type->FunctionType.ret_type);
    b32 returns_void = info.stars == 0 && !info.is_array
        && info.base_type->kind == NodeKind_Ident && cstring_cmp(info.base_type->Ident.token.str, "void") == 0;
    if (!returns_void)
    {
        gb_fprintf(p.out_file, " -> ");
        print_type(p, node->FunctionDecl.type->FunctionType.ret_type, 0);
    }

    gb_fprintf(p.out_file, " --- ");
}

void print_typedef(Printer p, Node *node, int indent)
{
    print_indent(p, indent);

    gbArray(Node *) defs = node->Typedef.var_list->VarDeclList.list;
    String renamed;
    for (int i = 0; i < gb_array_count(defs); i++)
    {
        if (defs[i]->no_print) continue;

        renamed = rename_ident(defs[i]->VarDecl.name->Ident.token.str, RENAME_TYPE, true, p.rename_map, p.conf, p.allocator);
        if ((defs[i]->VarDecl.type->kind == NodeKind_StructType
             || defs[i]->VarDecl.type->kind == NodeKind_UnionType
             || defs[i]->VarDecl.type->kind == NodeKind_EnumType)
            && defs[i]->VarDecl.type->StructType.name
            && !defs[i]->VarDecl.type->StructType.fields)
        {
            String record_renamed = rename_ident(defs[i]->VarDecl.type->StructType.name->Ident.token.str, RENAME_TYPE, true, p.rename_map, p.conf, p.allocator);
            if (string_cmp(record_renamed, renamed) == 0)
            {
                print_record(p, defs[i]->VarDecl.type, 0, true, true);
                if (i+1 < gb_array_count(defs))
                    gb_fprintf(p.out_file, ";\n\n");
                continue;
            }
        }

        if (defs[i]->VarDecl.type->kind == NodeKind_EnumType)
        {
            if (!defs[i]->VarDecl.type->EnumType.fields)
                gb_fprintf(p.out_file, "%.*s :: ", LIT(defs[i]->VarDecl.name->Ident.token.str));
            else if (!defs[i]->VarDecl.type->EnumType.name)
                defs[i]->VarDecl.type->EnumType.name = defs[i]->VarDecl.name;

            print_enum(p, defs[i]->VarDecl.type, indent, true, false);
            if (!defs[i]->VarDecl.type->EnumType.fields && i+1 < gb_array_count(defs))
                gb_fprintf(p.out_file, ";\n");
        }
        else
        {
            gb_fprintf(p.out_file, "%.*s :: %s",
                       LIT(renamed),
                       defs[i]->VarDecl.type->kind == NodeKind_FunctionType?"#type ":"");

            print_type(p, defs[i]->VarDecl.type, indent);
            if (i+1 < gb_array_count(defs))
                gb_fprintf(p.out_file, ";\n");
        }


    }
    gb_fprintf(p.out_file, ";\n\n");

}

void print_string(Printer p, String str, int indent)
{
    print_indent(p, indent);
    gb_fprintf(p.out_file, "%.*s", LIT(str));
}

void print_node(Printer p, Node *node, int indent, b32 top_level, b32 indent_first)
{
    if (node->no_print) return;

    switch (node->kind)
    {
        case NodeKind_Ident:
        print_ident(p, node, indent_first?indent:0);
        break;

        case NodeKind_FunctionDecl:
        print_function(p, node, indent_first?indent:0);
        gb_fprintf(p.out_file, ";\n%s", p.source_order?"\n":"");
        break;

        case NodeKind_VarDecl:
        print_variable(p, node, indent, top_level, 0);
        if (top_level) gb_fprintf(p.out_file, ";\n%s", p.source_order?"\n":"");
        break;

        case NodeKind_StructType:
        case NodeKind_UnionType:
        case NodeKind_EnumType:

        if (node->kind == NodeKind_EnumType)
            print_enum(p, node, indent, top_level, indent_first);
        else
            print_record(p, node, indent, top_level, indent_first);
        if (top_level) gb_fprintf(p.out_file, ";\n\n");
        break;

        case NodeKind_Typedef: {
            if (node->is_opaque)
            {
                TypeInfo info = get_type_info(node->Typedef.var_list->VarDeclList.list[0]->VarDecl.type);
                if (info.base_type->kind == NodeKind_EnumType)
                    print_enum(p, info.base_type, 0, true, true);
                else
                    print_record(p, info.base_type, 0, true, true);
                gb_fprintf(p.out_file, ";\n\n");
            }
            print_typedef(p, node, indent);
            //gb_fprintf(p.out_file, ";\n\n");
        } break;

        default:
        gb_printf_err("\x1b[31mERROR:\x1b[0m Unhandled Node in printer (%.*s)\n", LIT(node_strings[node->kind]));
        break;
    }
}

void print_define(Printer p, Node *def)
{
    // if (def->Define.value->kind == NodeKind_Invalid || def->Define.value->kind == NodeKind_SelectorExpr) return;
    String renamed = rename_ident(def->Define.name, RENAME_CONST, true, p.rename_map, p.conf, p.allocator);
    gb_fprintf(p.out_file, "%.*s :: ", LIT(renamed));
    print_expr(p, def->Define.value, 0);
    gb_fprintf(p.out_file, ";\n");
}

b32 _node_in_file(Printer p, Node *node)
{
    gb_printf("%.*s == %s?\n", LIT(node_token(node)->loc.file), p.file.filename);
    return cstring_cmp(node_token(node)->loc.file, p.file.filename) == 0;
}

void print_lib_variables(Printer p, Lib lib)
{
    String rename_temp;
    b32 found = false;
    for (int i = 0; i < gb_array_count(p.file.variables); i++)
    {
        if (p.conf->shallow_bind && !_node_in_file(p, p.file.variables[i])) continue;
        if (!hashmap_exists(lib.symbols, p.file.variables[i]->VarDecl.name->Ident.token.str)) continue;

        found = true;
        if (p.conf->var_case || (p.conf->var_prefix.len && !has_prefix(p.file.variables[i]->VarDecl.name->Ident.token.str, p.conf->var_prefix)))
            p.var_link_padding = gb_max(p.var_link_padding, p.file.variables[i]->VarDecl.name->Ident.token.str.len);
        rename_temp = rename_ident(p.file.variables[i]->VarDecl.name->Ident.token.str, RENAME_VAR, true, p.rename_map, p.conf, p.allocator);
        p.var_name_padding = gb_max(p.var_name_padding, rename_temp.len);
    }
    if (!found) return;

    if (p.conf->var_prefix.start && !p.conf->var_case)
        gb_fprintf(p.out_file, "@(link_prefix=\"%.*s\")\n", LIT(p.conf->var_prefix));
    gb_fprintf(p.out_file, "foreign %.*s {\n", LIT(lib.name));
    for (int i = 0; i < gb_array_count(p.file.variables); i++)
    {
        if (p.conf->shallow_bind && !_node_in_file(p, p.file.variables[i])) continue;
        if (!hashmap_exists(lib.symbols, p.file.variables[i]->VarDecl.name->Ident.token.str)) continue;
        print_node(p, p.file.variables[i], 1, true, true);
    }
    gb_fprintf(p.out_file, "}\n\n");
}

void print_lib_procs(Printer p, Lib lib)
{
    String rename_temp;
    b32 found = false;
    for (int i = 0; i < gb_array_count(p.file.functions); i++)
    {
        if (p.conf->shallow_bind && !_node_in_file(p, p.file.functions[i])) continue;
        if (!hashmap_exists(lib.symbols, p.file.functions[i]->FunctionDecl.name->Ident.token.str)) continue;
        found = true;

        rename_temp = rename_ident(p.file.functions[i]->FunctionDecl.name->Ident.token.str, RENAME_VAR, true, p.rename_map, p.conf, p.allocator);
        p.proc_name_padding = gb_max(p.proc_name_padding, rename_temp.len);

        if (p.conf->proc_case || (p.conf->proc_prefix.len && !has_prefix(p.file.functions[i]->FunctionDecl.name->Ident.token.str, p.conf->proc_prefix)))
            p.proc_link_padding = gb_max(p.proc_link_padding, p.file.functions[i]->FunctionDecl.name->Ident.token.str.len);
        else
            p.proc_name_padding = gb_max(p.proc_name_padding, rename_temp.len + p.proc_link_padding);
    }
    if (!found) return;

    if (p.conf->proc_prefix.start && !p.conf->proc_case)
        gb_fprintf(p.out_file, "@(link_prefix=\"%.*s\")\n", LIT(p.conf->proc_prefix));
    gb_fprintf(p.out_file, "foreign %.*s {\n", LIT(lib.name));

    for (int i = 0; i < gb_array_count(p.file.functions); i++)
    {
        if (p.conf->shallow_bind && _node_in_file(p, p.file.functions[i])) continue;
        if (!hashmap_exists(lib.symbols, p.file.functions[i]->FunctionDecl.name->Ident.token.str)) continue;
        print_node(p, p.file.functions[i], 1, true, true);
    }
    gb_fprintf(p.out_file, "}\n\n");
}

void print_file(Printer p)
{
    gb_fprintf(p.out_file, "package %.*s\n\n", LIT(p.package.name));
    for (int i = 0; i < gb_array_count(p.package.libs); i++)
        gb_fprintf(p.out_file, "foreign import %.*s \"system:%.*s\";\n", LIT(p.package.libs[i].name), LIT(p.package.libs[i].file));
    gb_fprintf(p.out_file, "\nimport _c \"core:c\"\n\n");

    if (p.source_order)
    {
//         for (int i = 0; i < gb_array_count(p.file.defines); i++)
//             if (!p.conf->shallow_bind || _node_in_file(p, p.file.defines[i]))
//                 print_define(p, p.file.defines[i]);

        gb_fprintf(p.out_file, "\n");
        for (int i = 0; i < gb_array_count(p.file.all_nodes); i++)
            if (!p.conf->shallow_bind || _node_in_file(p, p.file.all_nodes[i]))
                print_node(p, p.file.all_nodes[i], 0, true, true);
    }
    else
    {
        if (gb_array_count(p.needs_opaque_def) > 0)
        {
            gb_fprintf(p.out_file, "/* Opaque Types */\n");
            for (int i = 0; i < gb_array_count(p.needs_opaque_def); i++)
            {
                Node *type = p.needs_opaque_def[i];
                if (type->kind == NodeKind_EnumType)
                    print_enum(p, type, 0, true, true);
                else
                    print_record(p, type, 0, true, true);
                gb_fprintf(p.out_file, "\n");
            }
        }

//         if (gb_array_count(p.file.defines) > 0)
//         {
//             gb_fprintf(p.out_file, "/* Defines */\n");
//             for (int i = 0; i < gb_array_count(p.file.defines); i++)
//             {
//                 if ((!p.conf->shallow_bind || _node_in_file(p, p.file.defines[i]))
//                     && !p.file.defines[i]->no_print)
//                     print_define(p, p.file.defines[i]);
//             }
//             gb_fprintf(p.out_file, "\n");
//         }

        int record_count = gb_array_count(p.file.records);
        int tpdef_count  = gb_array_count(p.file.tpdefs);
        int ri = 0;
        int ti = 0;
        while (ri < record_count || ti < tpdef_count)
        {
            if (ti == tpdef_count || (ri < record_count && p.file.records[ri]->index < p.file.tpdefs[ti]->index))
            {
                if (!p.conf->shallow_bind || _node_in_file(p, p.file.records[ri]))
                    print_node(p, p.file.records[ri], 0, true, true);
                ri++;
            }
            else if (ri == record_count || (ti < tpdef_count && p.file.tpdefs[ti]->index < p.file.records[ri]->index))
            {
                top_level_type = true;
                if (!p.conf->shallow_bind || _node_in_file(p, p.file.tpdefs[ti]))
                    print_node(p, p.file.tpdefs[ti], 0, true, true);
                ti++;
                top_level_type = false;
            }
        }

        if (gb_array_count(p.file.variables) > 0)
        {
            gb_fprintf(p.out_file, "/* Variables */\n");
            for (int i = 0; i < gb_array_count(p.package.libs); i++)
                print_lib_variables(p, p.package.libs[i]);
        }

        if (gb_array_count(p.file.functions) > 0)
        {
            gb_fprintf(p.out_file, "/* Procedures */\n");
            for (int i = 0; i < gb_array_count(p.package.libs); i++)
                print_lib_procs(p, p.package.libs[i]);
        }
    }
}

void print_wrapper(Printer p);

void print_package(Printer p)
{
    for (int i = 0; i < gb_array_count(p.package.files); i++)
    {
        p.file = p.package.files[i];
        gbFile *out_file = gb_alloc_item(p.allocator, gbFile);

        create_path_to_file(p.file.output_filename);
        gb_file_create(out_file, p.file.output_filename);

        p.out_file = out_file;

        print_file(p);
        gb_file_close(p.out_file);

        /* if (p.wrap_conf->do_wrap) */
        /* { */
        /*     p.out_file = gb_alloc_item(p.allocator, gbFile); */

        /*     create_path_to_file(p.file.filename); */
        /*     gb_file_create(p.out_file, p.file.filename); */

        /*     print_wrapper(p); */
        /*     gb_file_close(p.out_file); */
        /* } */
    }
}

Node *child_type(Node *type)
{
    switch (type->kind)
    {
        case NodeKind_ArrayType:
        return type->ArrayType.type;
        case NodeKind_PointerType:
        return type->PointerType.type;
        case NodeKind_ConstType:
        return type->ConstType.type;
        default:
        return 0;
    }

}

u64 convert_cstrings(Printer p, Node *vars)
{
    u64 res = 0;
    gbArray(Node *) list = vars->VarDeclList.list;
    for (int i = 0; i < gb_array_count(list); i++)
    {
        TypeInfo ti = get_type_info(list[i]->VarDecl.type);
        if (ti.stars == 0 && !ti.is_array)
            continue;

        String str = integer_type_str(ti.base_type);
        if (cstring_cmp(str, "u8") == 0)
        {
            res |= (u64)1 << i;
        }
    }
    return res;
}
