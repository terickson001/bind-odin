#include "preprocess.h"
#include "util.h"
#include "expression.h"
#include "error.h"

#define peek_at(pp, n) (pp)->context->tokens.curr[n]
#define peek(pp) peek_at(pp, 0)

void pp_pop_context(Preprocessor *pp);

int pp_advance_n(Preprocessor *pp, int n)
{
    int popped = 0;
    isize start_line = peek(pp).loc.line;
    for (int i = 0; i < n; i++)
    {
        pp->context->tokens.curr++;
        while (&peek(pp) <= pp->context->tokens.end)
        {
            if (pp->context->line < peek(pp).loc.line)
                pp->context->line++;
            pp->context->column = peek(pp).loc.column;
            
            if (peek(pp).kind != Token_Comment)
                break;
            
            pp->context->tokens.curr++;
            if (&peek(pp) > pp->context->tokens.end && pp->context->next)
            {
                popped = pp->context->in_include?1:2;
                pp_pop_context(pp);
            }
        }
        
        if (&peek(pp) > pp->context->tokens.end && pp->context->next)
        {
            popped = pp->context->in_include ? 1 : 2;
            pp_pop_context(pp);
        }
    }
    
    return popped;
}
b32 pp_advance(Preprocessor *pp) { return pp_advance_n(pp, 1); }

void pp_retreat_n(Preprocessor *pp, int n)
{
    for (int i = 0; i < n; i++)
    {
        pp->context->tokens.curr--;
        while (&peek(pp) >= pp->context->tokens.end)
        {
            if (peek(pp).kind == Token_Comment)
                pp->context->tokens.curr--;
            else
                break;
        }
    }
    pp->line = peek(pp).loc.line;
}
void pp_retreat(Preprocessor *pp) { pp_retreat_n(pp, 1); }

void pp_push_context(Preprocessor *pp, Token_Run run, PP_Context context, char *file_contents)
{
    PP_Context *new_head = gb_alloc_item(pp->allocator, PP_Context);

    *new_head = context;
    new_head->next = pp->context;
    new_head->tokens = run;

    if (context.in_include && !context.in_macro && !context.in_macro)
    {
        gb_array_append(pp->file_contents, file_contents);
        gb_array_append(pp->file_tokens, run.start);
    }

    pp->context = new_head;
}

void pp_pop_context(Preprocessor *pp)
{
    PP_Context *old = pp->context;
    pp->context = old->next;

    if (old->in_macro)
        defines_destroy(old->local_defines);
    
    pp->end_of_prev = old->tokens.end;
    pp->paste_next = false;
    gb_free(pp->allocator, old);
}

Preprocessor *make_preprocessor(gbArray(Token) tokens, String root_dir, String filename, PreprocessorConfig *conf)
{
    gbAllocator alloc = gb_heap_allocator();
    Preprocessor *pp = gb_alloc_item(alloc, Preprocessor);

    Token_Run tokens_head = {tokens, tokens, tokens + gb_array_count(tokens)};
    PP_Context base_context = {0};
    base_context.filename = filename;
    base_context.line = 1;
    
    pp->context = gb_alloc_item(alloc, PP_Context);
    *pp->context = base_context;
    pp->context->tokens = tokens_head;
    
    gb_array_init(pp->file_contents, alloc);
    
    gb_array_init(pp->file_tokens, alloc);
    gb_array_append(pp->file_tokens, tokens);
    
    pp->line = 1;

    pp->allocator = alloc;
    pp->defines = gb_alloc_item(pp->allocator, Define_Map);
    defines_init(pp->defines, pp->allocator);

    pp->root_dir = root_dir;
    pp->conf = conf;

    gb_array_init(pp->output, alloc);

    init_std_defines(&pp->defines);

    pp->pragma_onces = hashmap_new(alloc);

    gbArray(String) include_files;
    if (conf->pre_includes && hashmap_get(conf->pre_includes, string_slice(filename, root_dir.len+1, -1), (void **)&include_files) == MAP_OK)
    {
        char path[512];
        for (int i = 0; i < gb_array_count(include_files); i++)
        {
            gbFileContents fc = {0};
            gb_snprintf(path, 512, "%.*s%c%.*s", LIT(root_dir), GB_PATH_SEPARATOR, LIT(include_files[i]));
            fc = gb_file_read_contents(pp->allocator, true, path);
            if (!fc.data)
                gb_printf_err("%.*s: \e[31mERROR:\e[0m Could not pre-include file '%s'\n",
                              LIT(filename), path);
            PP_Context context = {0};
            context.filename = make_string(gb_alloc_str(pp->allocator, path));
            context.line = 1;
            context.in_include = true;
            context.from_filename = filename;
            context.from_line = 0;
            context.in_sandbox = false;
            
            gbArray(Token) include_tokens;
            gb_array_init(include_tokens, pp->allocator);
            
            Tokenizer tokenizer = make_tokenizer(fc, make_string(context.filename.start));
            Token token;
            while ((token = get_token(&tokenizer)).kind != Token_EOF)
                gb_array_append(include_tokens, token);

            Token_Run run = {include_tokens, include_tokens, include_tokens+gb_array_count(include_tokens)-1};
            pp_push_context(pp, run, context, fc.data);
        }
    }
    return pp;
}

void destroy_preprocessor(Preprocessor *pp)
{
    if (pp->defines)
    {
        if (pp->defines->entries)
        {
            for (int i = 0; i < gb_array_count(pp->defines->entries); i++)
            {
                gb_free(pp->allocator, pp->defines->entries[i].value.file.start);
                if (pp->defines->entries[i].value.params)
                {
                    gb_array_free(pp->defines->entries[i].value.params);
                }
            }
        }
        defines_destroy(pp->defines);
        gb_free(pp->allocator, pp->defines);
    }

    for (int i = 0; i < gb_array_count(pp->file_contents); i++)
        gb_free(pp->allocator, pp->file_contents[i]);
    gb_array_free(pp->file_contents);

    for (int i = 0; i < gb_array_count(pp->file_tokens); i++)
        gb_array_free(pp->file_tokens[i]);
    gb_array_free(pp->file_tokens);

    if (pp->output)
        gb_array_free(pp->output);
    gb_free(pp->allocator, pp);
}


void pp_parse_macro_args(Preprocessor *pp, gbArray(Token_Run) *args, b32 is_params)
{
    b32 free_args = false;
    if (!*args)
    {
        gb_array_init(*args, pp->allocator);
        free_args = true;
    }
    
    expect_token(&pp->context->tokens, Token_OpenParen);
    for (;;)
    {
        Token_Run curr = {&peek(pp), &peek(pp), 0};
        int skip_parens = 0;
        while (peek(pp).kind != Token_Comma &&
               peek(pp).kind != Token_CloseParen)
        {
            if (peek(pp).kind == Token_OpenParen)
            {
                skip_parens = 0;
                do {
                    if (peek(pp).kind == Token_OpenParen) skip_parens++;
                    else if (peek(pp).kind == Token_CloseParen) skip_parens--;
                    pp_advance(pp);
                } while (skip_parens > 0);
            }
            else
            {
                pp_advance(pp);
            }
        }
        curr.end = &peek_at(pp, -1);
        if (is_params && curr.start == curr.end && curr.start[0].kind == Token_Ellipsis)
            curr = make_token_run("__VA_ARGS__", Token_Ident);
        if (!(curr.start[-1].kind == Token_OpenParen
              && curr.end[1].kind == Token_CloseParen
              && curr.end < curr.start
              && is_params))
            gb_array_append(*args, curr);
        if (peek(pp).kind == Token_CloseParen)
            break;
        pp_advance(pp);
    }
    expect_token(&pp->context->tokens, Token_CloseParen);
    // if (free_args)
    //     gb_array_free(*args);
}

gbArray(Token) run_pp_sandboxed(Preprocessor *pp, Token_Run *run);

void pp_write_token_run(Preprocessor *pp, Token_Run to_write)
{
    // if (pp->context->in_include && !pp->context->in_sandbox) // Don't actually write things from includes
    //     return;

    while (to_write.curr <= to_write.end)
    {
        Token tok = to_write.curr[0];
        if (tok.kind == Token_Comment ||
            tok.kind == Token_BackSlash)
        {
            to_write.curr++;
            continue;
        }

        isize line = tok.loc.line;
        isize col = tok.loc.column;
        isize line_diff, col_diff;
        if (!pp->context->prev_token && !pp->context->preceding_token)
        {
            line_diff = line;
            col_diff  = col;
        }
        else if (pp->context->in_macro && !pp->context->prev_token)
        {
            line_diff = pp->context->from_line - pp->context->preceding_token->loc.line;
            if (line_diff)
                col_diff = pp->context->from_column;
            else
                col_diff = pp->context->from_column - (pp->context->preceding_token->loc.column + pp->context->preceding_token->str.len);
        }
        else
        {
            line_diff = line - pp->context->prev_token->loc.line;
            if (line_diff)
                col_diff = col;
            else
                col_diff = col - (pp->context->prev_token->loc.column + pp->context->prev_token->str.len);
        }

        if (pp->paste_next)
        {
            line_diff = 0;
            col_diff  = 0;
            pp->paste_next = false;

            Token *last = &pp->output[gb_array_count(pp->output)-1];

            String new = {gb_alloc(pp->allocator, last->str.len+tok.str.len+1), last->str.len+tok.str.len};
            gb_snprintf(new.start, new.len, "%.*s%.*s", LIT(last->str), LIT(tok.str));

            last->str = new;

            Define def = pp_get_define(pp, new);
            if (def.in_use)
            {
                gb_array_resize(pp->output, gb_array_count(pp->output)-1);
                Token_Run macro = {last, last, last};
                gbArray(Token) output = run_pp_sandboxed(pp, &macro);
                if (output && gb_array_count(output) > 0)
                {
                    Token_Run run = {output, output, output+gb_array_count(output)-1};
                    pp_write_token_run(pp, run);
                }
            }
            else
            {
                pp->write_column += tok.str.len;
            }
            
            to_write.curr++;
            continue;
        }

        pp->write_line += line_diff;
        if (line_diff)
            pp->write_column = col_diff;
        else
            pp->write_column += col_diff;
        tok.pp_loc.line   = pp->write_line;
        tok.pp_loc.column = pp->write_column;

        if (pp->stringify_next ||
            (pp->context->in_macro && pp->context->stringify))
        {
            tok.pp_loc.column++;
            pp->write_column++;
            tok.kind = Token_String;
            pp->stringify_next = false;
        }

        gb_array_append(pp->output, tok);
        pp->write_column += tok.str.len;
        pp->context->prev_token = to_write.curr;
        to_write.curr++;
    }
}

int pp_next_ident_or_directive(Preprocessor *pp, b32 should_write)
{
    isize start_line = peek(pp).loc.line;
    
    Token_Run to_write = {&peek(pp), &peek(pp), 0};
    for (;;)
    {
        if (&peek(pp)> pp->context->tokens.end || peek(pp).kind == Token_EOF)
        {
            if (pp->context->next)
            {
                to_write.end = gb_min(&peek_at(pp, -1), pp->context->tokens.end);
                if (should_write)
                    pp_write_token_run(pp, to_write);
                pp_pop_context(pp);
                to_write.start = &peek(pp);
                to_write.curr = to_write.start;
                to_write.end = 0;
                continue;
            }
            else
            {
                to_write.end = &peek_at(pp, -1);
                if (should_write)
                    pp_write_token_run(pp, to_write);
                return 0;
            }
        }
        else if (peek(pp).kind == Token_Paste && should_write)
        {
            to_write.end = &peek_at(pp, -1);
            if (should_write)
                pp_write_token_run(pp, to_write);
            to_write.start = &peek_at(pp, 1);
            to_write.curr = to_write.start;
            pp->paste_next = true;
        }
        else if (peek(pp).kind == Token_Hash ||
                 peek(pp).kind == Token_Ident ||
                 peek(pp).kind == Token_pragma)
        {
            break;
        }
        pp->context->tokens.curr++;
    }
    
    to_write.end = &peek_at(pp, -1);
    
    if (should_write)
        pp_write_token_run(pp, to_write);

    if (peek(pp).kind == Token_Hash)
    {
        pp->context->tokens.curr++;
        return 1;
    }

    if (peek(pp).kind == Token_pragma)
        return 1;

    if (peek(pp).kind == Token_Ident)
        return 2;
    
    return 0;
}

void pp_push_cond(Preprocessor *pp, b32 skip_else)
{
    Cond_Stack *cond = gb_alloc_item(pp->allocator, Cond_Stack);
    cond->skip_else = skip_else;
    cond->next = pp->conditionals;
    pp->conditionals = cond;
}

Define *pp_unique_defines(Preprocessor *pp, String name)
{
    if (cstring_cmp(name, "__LINE__") == 0)
    {
        char *line_str = gb_alloc(pp->allocator, 24);
        gb_snprintf(line_str, 64, "%ld", pp->line);
        return &(Define){1, name, make_token_run(line_str, Token_Integer), 0, pp->context->filename, pp->line};
    }
    else if (cstring_cmp(name, "__FILE__") == 0)
    {
        char *file_str = gb_alloc_str_len(pp->allocator, pp->context->filename.start, pp->context->filename.len);
        return &(Define){1, name, make_token_run(file_str, Token_String), 0, pp->context->filename, pp->line};
    }
    return 0;
}

Define *pp_custom_symbol(Preprocessor *pp, String name)
{
    String *val;
    if (pp->conf->custom_symbols
     && hashmap_get(pp->conf->custom_symbols, name, (void **)&val) == MAP_OK)
    {
        Token_Run run = str_make_token_run(*val, Token_String);
        run.start->loc.file = pp->context->filename;
        return &(Define){1, name, str_make_token_run(*val, Token_Ident), 0, pp->context->filename, pp->line};
    }
    return 0;
}

Define pp_get_define(Preprocessor *pp, String name)
{
    Define *define = 0;

    define = pp_unique_defines(pp, name);
    if (!define && pp->conf->custom_symbols)
        define = pp_custom_symbol(pp, name);
    if (!define && pp->context && pp->context->local_defines)
        define = get_define(pp->context->local_defines, name);
    if (!define)
        define = get_define(pp->defines, name);
    if (define)
        return *define;
    else
        return (Define){0};
}

void pp_do_macro(Preprocessor *pp, Define define, Token name, Token *preceding_token)
{
    String invocation = name.str;
    b32 has_va_args = false;
    Token_Run va_args = {0};

    
    gbArray(Token_Run) args;
    if (define.params)
    {
        gb_array_init(args, pp->allocator);
        if (cstring_cmp(name.str, "__VA_OPT__") == 0)
        {
            Token_Run arg = {&peek_at(pp, 1), &peek_at(pp, 1), 0};
            
            int skip_parens = 0;
            do {
                if (peek(pp).kind == Token_OpenParen) skip_parens++;
                else if (peek(pp).kind == Token_CloseParen) skip_parens--;
                pp_advance(pp);
            } while (skip_parens > 0);
            arg.end = &peek_at(pp, -2);
            gb_array_append(args, arg);
        }
        else
        {
            pp_parse_macro_args(pp, &args, false);
            if (gb_array_count(args) == 1
                && gb_array_count(define.params) == 0
                && args[0].end < args[0].start)
            {
                gb_array_free(args);
                gb_array_init(args, pp->allocator);
            }
            else if (cstring_cmp(token_run_string(define.params[gb_array_count(define.params)-1]), "__VA_ARGS__") == 0)
            {
                has_va_args = true;
                if (gb_array_count(args) >= gb_array_count(define.params))
                {
                    va_args.start = args[gb_array_count(define.params)-1].start;
                    va_args.curr = va_args.start;
                    va_args.end = args[gb_array_count(args)-1].end;
                }
                gb_array_resize(args, gb_array_count(define.params)-1);
                gb_array_append(args, va_args);
            }
        }

        invocation.len = (peek_at(pp, -1).str.start - invocation.start) + 1;
        if (gb_array_count(args) != gb_array_count(define.params))
            gb_printf_err("(%.*s:%ld): \e[31mERROR:\e[0m Expected %ld arguments, but got %ld in macro '%.*s'\n",
                          LIT(pp->context->filename), pp->line,
                          gb_array_count(define.params), gb_array_count(args),
                          LIT(invocation));
        GB_ASSERT(gb_array_count(args) == gb_array_count(define.params));
    }
    pp->context->prev_token = &peek_at(pp, -1);

    isize from_column = name.loc.column;
    isize from_line = name.loc.line;
    if (pp->context->in_macro && pp->context->tokens.start->str.start == name.str.start)
    {
        from_column = (name.loc.column-pp->context->macro.value.start->loc.column) + pp->context->from_column;
        from_line = pp->context->from_line;
    }
    
    PP_Context new_context = {0};
    new_context.in_macro = true;
    new_context.in_include = pp->context->in_include;
    new_context.macro = define;
    new_context.macro_invocation = invocation;
    new_context.filename = pp->context->filename;
    new_context.from_line = from_line;
    new_context.from_column = from_column;
    new_context.preceding_token = preceding_token;
    new_context.in_sandbox = pp->context->in_sandbox;
    if (pp->stringify_next || (pp->context->stringify))
    {
        new_context.stringify = true;
        pp->stringify_next = false;
    }
    

    Define_Map *local_defines;
    local_defines = gb_alloc_item(pp->allocator, Define_Map);
    defines_init(local_defines, pp->allocator);

    if (define.params)
    {
        for (int i = 0; i < gb_array_count(define.params); i++)
        {
            Token_Run processed_run = args[i];
            if (pp->context->in_macro && args[i].start)
            {
                gbArray(Token) tokens = run_pp_sandboxed(pp, &args[i]);
                processed_run = (Token_Run){tokens, tokens, tokens+gb_array_count(tokens)-1};
                if (processed_run.end < processed_run.start)
                    processed_run = (Token_Run){0};
            }
            
            add_define(&local_defines, token_run_string(define.params[i]), processed_run, 0, pp->line, pp->context->filename);
        }
    }

    if (has_va_args)
    {
        Token_Run va_opt_value;
        if (va_args.start)
            va_opt_value = make_token_run("x", Token_Ident);
        else
            va_opt_value = make_token_run("", Token_Ident);
        
        gbArray(Token_Run) va_opt_params;
        gb_array_init(va_opt_params, pp->allocator);
        gb_array_append(va_opt_params, make_token_run("x", Token_Ident));
        add_define(&local_defines, make_string("__VA_OPT__"), va_opt_value, va_opt_params, pp->line, pp->context->filename);
    }

    add_fake_define(&local_defines, define.key);
    new_context.local_defines = local_defines;

    pp_push_context(pp, define.value, new_context, 0);
}

gbArray(Token) run_pp_sandboxed(Preprocessor *pp, Token_Run *run)
{
    gbArray(Token) new_output = 0;
    gb_array_init(new_output, pp->allocator);

    Preprocessor *temp_pp = gb_alloc_copy(pp->allocator, pp, sizeof(Preprocessor));
    temp_pp->context = 0;
    temp_pp->output = new_output;
    temp_pp->conditionals = 0;
    temp_pp->stringify_next = false;
    temp_pp->paste_next = false;

    PP_Context new_context = *pp->context;
    new_context.in_sandbox = true;
    new_context.stringify = false;
    new_context.no_paste = true;
    
    gb_array_init(temp_pp->file_contents, pp->allocator);
    gb_array_init(temp_pp->file_tokens, pp->allocator);
    
    pp_push_context(temp_pp, *run, new_context, 0);
    run_pp(temp_pp);

    gbArray(Token) output_ret = temp_pp->output;
    gb_free(pp->allocator, temp_pp);
    
    return output_ret;
}

gbArray(Token) pp_do_sandboxed_macro(Preprocessor *pp, Token_Run *run, Define define, Token name)
{
    gbArray(Token) new_output = 0;
    gb_array_init(new_output, pp->allocator);

    Preprocessor *temp_pp = gb_alloc_copy(pp->allocator, pp, sizeof(Preprocessor));
    temp_pp->context = 0;
    temp_pp->output = new_output;
    temp_pp->conditionals = 0;
    temp_pp->stringify_next = false;
    temp_pp->paste_next = false;
    
    gb_array_init(temp_pp->file_contents, pp->allocator);
    gb_array_init(temp_pp->file_tokens, pp->allocator);

    PP_Context context = {0};
    context.filename = pp->context->filename;
    context.in_sandbox = true;

    if (run)
        pp_push_context(temp_pp, *run, context, 0);
    pp_do_macro(temp_pp, define, name, 0);
    run_pp(temp_pp);

    gbArray(Token) output_ret = temp_pp->output;
    gb_free(pp->allocator, temp_pp);

    return output_ret;
}


Token_Run pp_get_line(Preprocessor *pp)
{
    isize curr_line = peek(pp).loc.line;

    if (peek(pp).kind == Token_Comment)
    {
        pp_advance(pp);
        if (peek(pp).loc.line > curr_line)
            return (Token_Run){0};
    }
    
    Token_Run line = {&peek(pp), &peek(pp), 0};
    int popped = 0;
    Token *save = &peek(pp);
    
    while (peek(pp).loc.line <= curr_line && peek(pp).kind != Token_EOF)
    {
        if (peek(pp).kind == Token_BackSlash)
            curr_line++;
        save = &peek(pp);
        popped = pp_advance(pp);
        if (popped)
            break;
    }
    
    line.end = save;
    
    return line;
}

void _directive_skip_conditional_block(Preprocessor *pp, b32 skip_all)
{
    int skip_endifs = 0;
    for (;;)
    {
        // while not directive, consume and skip to next
        while (pp_next_ident_or_directive(pp, false) != 1) { pp_advance(pp); }
        String directive = expect_tokens(&pp->context->tokens, 4,
                                         Token_Ident,
                                         Token_if,
                                         Token_else,
                                         Token_pragma).str;
        if (cstring_cmp(directive, "if") == 0 ||
            cstring_cmp(directive, "ifdef") == 0 ||
            cstring_cmp(directive, "ifndef") == 0)
        {
            skip_endifs++;
        }
        else if (!skip_all && skip_endifs == 0 &&
                 (cstring_cmp(directive, "else") == 0 ||
                  cstring_cmp(directive, "elif") == 0 ||
                  cstring_cmp(directive, "endif") == 0))
        {
            pp_retreat_n(pp, 2);
            break;
        }
        else if (skip_endifs != 0 && cstring_cmp(directive, "endif") == 0)
        {
            skip_endifs--;
        }
        else if (skip_all && skip_endifs == 0
                 && cstring_cmp(directive, "endif") == 0)
        {
            pp_retreat_n(pp, 2);
            break;
        }
    }
}

void _directive_conditional(Preprocessor *pp, b32 test_res, b32 is_else)
{
    if (test_res)
    {
        if (!is_else) pp_push_cond(pp, true);
        else pp->conditionals->skip_else = true;
    }
    else
    {
        if (!is_else) pp_push_cond(pp, false);
        _directive_skip_conditional_block(pp, false);
    }   
}

void directive_define(Preprocessor *pp)
{
    Token def_token = {0};
    for (int k = Token__KeywordBegin+1; k < Token__KeywordEnd; k++)
    {
        def_token = accept_token(&pp->context->tokens, k);
        if (def_token.str.start)
            break;
    }

    if (!def_token.str.start)
        def_token = expect_token(&pp->context->tokens, Token_Ident);
    String def_name = def_token.str;

    gbArray(Token_Run) params = 0;
    if (peek(pp).kind == Token_OpenParen &&
        peek(pp).loc.column == def_token.loc.column + def_token.str.len)
    {
        gb_array_init(params, pp->allocator);
        pp_parse_macro_args(pp, &params, true);
    }
    isize def_line = def_token.loc.line;
    Token_Run value = {0};
    if (def_line == peek(pp).loc.line)
    {
        value = pp_get_line(pp);
    }
    
    add_define(&pp->defines, def_name, value, params, def_line, pp->context->filename);
}

void directive_undef(Preprocessor *pp)
{
    Token def_token = {0};
    for (int k = Token__KeywordBegin+1; k < Token__KeywordEnd; k++)
    {
        def_token = accept_token(&pp->context->tokens, k);
        if (def_token.str.start)
            break;
    }

    if (!def_token.str.start)
        def_token = expect_token(&pp->context->tokens, Token_Ident);
    String def_name = def_token.str;

    // String name = expect_token(&pp->context->tokens, Token_Ident).str;
    remove_define(&pp->defines, def_name);
}

void _directive_include(Preprocessor *pp, b32 next)
{
    String filename;
    isize from_line = pp->line;
    b32 local_first;
    if (peek(pp).kind == Token_Lt)
    {
        local_first = false;
        pp_advance(pp);
        Token_Run filename_run;
        filename_run.start = &peek(pp);
        filename_run.curr = filename_run.start;
        while (peek(pp).kind != Token_Gt)
            pp_advance(pp);
        filename_run.end = &peek_at(pp, -1);
        filename = token_run_string(filename_run);
        expect_token(&pp->context->tokens, Token_Gt);
    }
    else if (peek(pp).kind == Token_String)
    {
        local_first = true;
        Token filename_token = expect_token(&pp->context->tokens, Token_String);
        filename = filename_token.str;
    }
    else
    {
        Token tok = expect_token(&pp->context->tokens, Token_Ident);
        Define def = pp_get_define(pp, tok.str);
        if (!def.in_use)
        {
            error(tok, "Undefined identifier '%.*s' in \e[35m#include\e[0m directive", LIT(tok.str));
            gb_exit(1);
        }
        
        if (def.params && peek(pp).kind == Token_OpenParen)
        {
            Token_Run macro = {&peek(pp), &peek(pp), &peek(pp)};
            if (macro.end[0].kind == Token_OpenParen)
            {
                while(macro.end[0].kind != Token_CloseParen)
                    macro.end++;
            }
            pp->context->tokens.curr = macro.end+1;
            gbArray(Token) res = pp_do_sandboxed_macro(pp, &macro, def, tok);
            filename = token_run_string(alloc_token_run(res, gb_array_count(res)));
        }
        else
        {
            if (def.value.curr[0].kind == Token_Lt)
            {
                local_first = false;
                def.value.curr++;
                Token_Run filename_run;
                filename_run.start = def.value.curr;
                filename_run.curr = filename_run.start;
                while (def.value.curr[0].kind != Token_Gt)
                    def.value.curr++;
                filename_run.end = def.value.curr-1;
                filename = token_run_string(filename_run);
                expect_token(&def.value, Token_Gt);
            }
            else if (def.value.curr[0].kind == Token_String)
            {
                local_first = true;
                filename = expect_token(&def.value, Token_String).str;
            }
            else
            {
                error(tok, "Macro '%.*s' expands to invalid value '%.*s' for \e[35m#include\e[0m directive", LIT(tok.str), LIT(token_run_string(def.value)));
                gb_exit(1);
            }
        }
    }

    String root_dir = dir_from_path(pp->context->filename);

    char path[512];
    gbFileContents fc = {0};
    if (local_first && !next)
    {
        gb_snprintf(path, 512, "%.*s%c%.*s", LIT(root_dir), root_dir.len?GB_PATH_SEPARATOR:0, LIT(filename));
        fc = gb_file_read_contents(pp->allocator, true, path);
    }
    if (pp->conf->include_dirs)
    {
        for (int i = 0; i < gb_array_count(pp->conf->include_dirs) && !fc.data; i++)
        {
            gb_snprintf(path, 512, "%.*s%c%.*s", LIT(pp->conf->include_dirs[i]), GB_PATH_SEPARATOR, LIT(filename));
            if (!next || !has_prefix(make_string(path), root_dir))
                fc = gb_file_read_contents(pp->allocator, true, path);
        }
    }
    for (int i = 0; i < gb_array_count(pp->system_includes) && !fc.data; i++)
    {
        gb_snprintf(path, 512, "%.*s%c%.*s", LIT(pp->system_includes[i]), GB_PATH_SEPARATOR, LIT(filename));
        if (!next || !has_prefix(make_string(path), root_dir))
            fc = gb_file_read_contents(pp->allocator, true, path);
    }
    
    PP_Context context = {0};
    context.filename = make_string(gb_alloc_str(pp->allocator, path));
    context.line = 1;
    context.in_include = true;
    context.from_filename = pp->context->filename;
    context.from_line = from_line;
    context.in_sandbox = pp->context->in_sandbox;

    gbArray(Token) tokens;

    if (fc.data)
    {
        if (hashmap_exists(pp->pragma_onces, context.filename))
            return;
        Tokenizer tokenizer = make_tokenizer(fc, make_string(context.filename.start));
        gb_array_init(tokens, pp->allocator);
        Token token;
        while ((token = get_token(&tokenizer)).kind != Token_EOF)
            gb_array_append(tokens, token);
    }
    else
    {
        Token tok = {.loc={.file=pp->context->filename, .line=from_line}};
        error(tok, "Could not \e[35m#include\e[0m file '%.*s'(%s)", LIT(filename), path);
        gb_exit(1);
    }
    
    Token_Run run = {tokens, tokens, tokens+gb_array_count(tokens)-1};
    pp_push_context(pp, run, context, fc.data);
}

void directive_include(Preprocessor *pp)
{
    _directive_include(pp, false);
}

void directive_include_next(Preprocessor *pp)
{
    _directive_include(pp, true);
}

void directive_ifdef(Preprocessor *pp, b32 invert)
{
    String def_name = expect_token(&pp->context->tokens, Token_Ident).str;
    Define define = pp_get_define(pp, def_name);
    
    _directive_conditional(pp, define.in_use ^ invert, false);
}

void directive_if(Preprocessor *pp)
{
    Token_Run expr = pp_get_line(pp);

    Expr *parsed = pp_parse_expression(expr, pp->allocator, pp, true);
    Expr *res = pp_eval_expression(parsed);

    GB_ASSERT(res && res->kind == Expr_Constant && res->constant.is_set);

    _directive_conditional(pp, res->constant.val, false);
    
    free_expr(pp->allocator, res);
    free_expr(pp->allocator, parsed);
}
    
void directive_elif(Preprocessor *pp)
{
    if (pp->conditionals->skip_else)
    {
        Token_Run expr = pp_get_line(pp);
        _directive_skip_conditional_block(pp, true);
    }
    else
    {
        Token_Run expr = pp_get_line(pp);

        Expr *parsed = pp_parse_expression(expr, pp->allocator, pp, true);
        Expr *res = pp_eval_expression(parsed);
        
        GB_ASSERT(res && res->kind == Expr_Constant && res->constant.is_set);
        _directive_conditional(pp, res->constant.val, true);
        free_expr(pp->allocator, res);
    }
}

void directive_else(Preprocessor *pp)
{
    if (pp->conditionals->skip_else)
        _directive_skip_conditional_block(pp, true);
}

void directive_endif(Preprocessor *pp)
{
    Cond_Stack *old_cond = pp->conditionals;
    pp->conditionals = pp->conditionals->next;
    gb_free(pp->allocator, old_cond);
}

void directive_error(Preprocessor *pp)
{
    isize line = pp->line;
    Token_Run message = pp_get_line(pp);
    gb_printf_err("%.*s:%ld: \e[31mError:\e[0m %.*s\n", LIT(pp->context->filename), pp->line, LIT(token_run_string(message)));
}

void directive_warning(Preprocessor *pp)
{
    isize line = pp->line;
    Token_Run message = pp_get_line(pp);
    gb_printf_err("%.*s:%ld: \e[33mWarning:\e[0m %.*s\n", LIT(pp->context->filename), line, LIT(token_run_string(message)));
}

void directive_line(Preprocessor *pp) {}

void directive_pragma(Preprocessor *pp)
{
    Token_Run pragma = pp_get_line(pp);
    
    if (cstring_cmp(pragma.start->str, "once") == 0)
        hashmap_put(pp->pragma_onces, pp->context->filename, 0);
    // else
    //     gb_printf("(%.*s:%ld): \e[32mNOTE:\e[0m '#pragma %.*s' skipped\n",
    //               LIT(pp->context->filename),
    //               pp->line,
    //               LIT(token_run_string(pragma)));
}

void keyword_pragma(Preprocessor *pp)
{
    Token_Run pragma = {&peek_at(pp, 1), &peek_at(pp, 1), 0};
                
    int skip_parens = 0;
    do {
        if (peek(pp).kind == Token_OpenParen) skip_parens++;
        else if (peek(pp).kind == Token_CloseParen) skip_parens--;
        pp_advance(pp);
    } while (skip_parens > 0);
    pragma.end = &peek_at(pp, -2);
    
    // Token_Run pragma = pp_get_line(pp);
    
    if (cstring_cmp(pragma.start->str, "once") == 0)
        hashmap_put(pp->pragma_onces, pp->context->filename, 0);
    // else
    //     gb_printf("(%.*s:%ld): \e[32mNOTE:\e[0m '#pragma %.*s' skipped\n",
    //               LIT(pp->context->filename),
    //               pp->line,
    //               LIT(token_run_string(pragma)));
}

void run_pp(Preprocessor *pp)
{
    while (true)
    {
        if (peek(pp).kind == Token_EOF || &peek(pp) > pp->context->tokens.end)
        {
            if (pp->context->next)
                pp_pop_context(pp);
            else
                break;
        }
        int type = pp_next_ident_or_directive(pp, true);
        if (type == 1) // directive
        {
            Token *reset = &peek(pp);
            String directive = expect_tokens(&pp->context->tokens, 4,
                                             Token_Ident,
                                             Token_if,
                                             Token_else,
                                             Token_pragma).str;
            if (cstring_cmp(directive, "define") == 0)
            {
                directive_define(pp);
            }
            else if (cstring_cmp(directive, "undef") == 0)
            {
                directive_undef(pp);
            }
            else if (cstring_cmp(directive, "include") == 0)
            {
                directive_include(pp);
            }
            else if (cstring_cmp(directive, "include_next") == 0)
            {
                directive_include_next(pp);
            }
            else if (cstring_cmp(directive, "ifdef") == 0)
            {
                directive_ifdef(pp, false);
            }
            else if (cstring_cmp(directive, "ifndef") == 0)
            {
                directive_ifdef(pp, true);
            }
            else if (cstring_cmp(directive, "if") == 0)
            {
                directive_if(pp);
            }
            else if (cstring_cmp(directive, "elif") == 0)
            {
                directive_elif(pp);
            }
            else if (cstring_cmp(directive, "else") == 0)
            {
                directive_else(pp);
            }
            else if (cstring_cmp(directive, "endif") == 0)
            {
                directive_endif(pp);
            }
            else if (cstring_cmp(directive, "error") == 0)
            {
                directive_error(pp);
            }
            else if (cstring_cmp(directive, "warning") == 0)
            {
                directive_warning(pp);
            }
            else if (cstring_cmp(directive, "line") == 0)
            {
                directive_line(pp);
            }
            else if (cstring_cmp(directive, "pragma") == 0)
            {
                directive_pragma(pp);
            }
            else if (cstring_cmp(directive, "__pragma") == 0)
            {
                keyword_pragma(pp);
            }
            else if (pp->context->in_macro)
            {
                pp->context->tokens.curr = reset;
                pp->stringify_next = true;
            }
            else
            {
                gb_printf_err("%.*s:%ld: \e[33mWarning:\e[0m Unhandled directive (%.*s)\n", LIT(pp->context->filename), pp->line, LIT(directive));
            }
        }
        else if (type == 2) // Identifier
        {
            Token *reset = &peek(pp);
            Token ident = expect_token(&pp->context->tokens, Token_Ident);
            
            Define define = pp_get_define(pp, ident.str);
            
            if (define.in_use && !(define.params && peek(pp).kind != Token_OpenParen))
            {
                if (!define.value.start)
                {
                    if (define.params)
                    {
                        gbArray(Token_Run) args_temp;
                        gb_array_init(args_temp, pp->allocator);
                        pp_parse_macro_args(pp, &args_temp, false);
                        gb_array_free(args_temp);
                    }
                    else if (pp->paste_next
                        && cstring_cmp(define.key, "__VA_ARGS__") == 0
                        && pp->output[gb_array_count(pp->output)-1].kind == Token_Comma)
                    {
                        gb_array_resize(pp->output, gb_array_count(pp->output)-1);
                        pp->paste_next = false;
                    }
                    continue;
                }
                Token *preceding_token;
                if (reset == pp->context->tokens.start && pp->context->in_macro)
                    preceding_token = pp->context->preceding_token;
                else if (pp->context->in_macro)
                    preceding_token = pp->context->prev_token;
                else
                    preceding_token = reset-1;
                pp_do_macro(pp, define, ident, preceding_token);
            }
            else
            {
                pp_write_token_run(pp, (Token_Run){reset, reset, reset});
            }
        }
        else
        {
            break;
        }
    }
}

gbArray(Define) pp_dump_defines(Preprocessor *pp, String whitelist_dir)
{
    gbArray(Define) defines = get_define_list(pp->defines, whitelist_dir, pp->allocator);
    gbArray(Token) output;
    for (int i = 0; i < gb_array_count(defines); i++)
    {
        output = run_pp_sandboxed(pp, &defines[i].value);
        defines[i].value = (Token_Run){output, output, output+gb_array_count(output)-1};
    }

    return defines;
}

void pp_print(Preprocessor *pp, char *filename)
{
    gbAllocator a = pp->allocator;
    
    gbFile *pp_out_file = gb_alloc_item(a, gbFile);
    create_path_to_file(filename);
    gb_file_create(pp_out_file, filename);

    Token prev_token = {0};
    String prev_token_string = {0};
    i32 prev_token_length = 0;

    for (int i = 0; i < gb_array_count(pp->output); i++)
    {
        Token token = pp->output[i];

        String token_string = token.str;

        i32 newline = 0;
        i32 num_spaces = 0;
        if (i > 0)
        {
            newline = token.pp_loc.line - prev_token.pp_loc.line;
            if (!newline)
                num_spaces = token.pp_loc.column - (prev_token.pp_loc.column+prev_token.str.len);
        }

        char *newlines = gb_alloc(a, newline+1);
        gb_memset(newlines, '\n', newline);

        char *spaces = gb_alloc(a, num_spaces+1);
        gb_memset(spaces, ' ', num_spaces);
            
        char *indentation = 0;
        if (newline && token.pp_loc.column > 0)
        {
            indentation = gb_alloc(a, token.pp_loc.column+1);
            gb_memset(indentation, ' ', token.pp_loc.column);
        }

        b32 is_string = token.kind == Token_String;
        b32 is_char = token.kind == Token_Char;
 
        /* gb_fprintf(pp_out_file, "%s%s%s%c%.*s%c", */
        /*            newlines, */
        /*            indentation, */
        /*            spaces, */
        /*            is_string?'"':is_char?'\'':0, */
        /*            LIT(token_string), */
        /*            is_string?'"':is_char?'\'':0); */
            
        gb_fprintf(pp_out_file, "%c%c%s%s%c%.*s%c",
                   newline?'\n':0,
                   newline > 1?'\n':0,
                   indentation,
                   spaces,
                   is_string?'"':is_char?'\'':0,
                   LIT(token_string),
                   is_string?'"':is_char?'\'':0);
            
        gb_free(a, newlines);
        gb_free(a, spaces);
        prev_token = token;
    }
    gb_file_close(pp_out_file);
}
