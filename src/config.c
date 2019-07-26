#include "config.h"

typedef struct Reader
{
    char *data;
    char *file;
    isize line, col;
    
    Config *conf;
    
    gbAllocator alloc;
} Reader;

void reader_advance_n(Reader *r, int n)
{
    for (int i = 0; i < n; i++)
    {
        if (r->data[0] == '\n')
        {
            r->line++;
            r->col = 0;
        }
        else
        {
            r->col++;
        }
        r->data++;
    }
}
void reader_advance(Reader *r)
{
    reader_advance_n(r, 1);
}

void reader_error(Reader *r, char const *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    gb_printf_err("%s(%ld:%ld): \e[31mERROR:\e[0m %s\n",
                  r->file, r->line, r->col,
                  gb_bprintf_va(fmt, va));
    va_end(va);
    gb_exit(1); // Just exit for now, because we have no way for skipping past the error
}

void require_c(Reader *r, char c, char *desc)
{
    if (r->data[0] != c)
        reader_error(r, "Expected %s (%c), got '%c'\n", desc, c, r->data[0]);
    reader_advance(r);
}

void skip_to_eol(Reader *r)
{
    while (r->data[0] != '\n' && r->data[0] != '\r')
        reader_advance(r);
}

void skip_comment(Reader *r)
{
    if (r->data[1] == '>')
    {
        while(gb_strncmp(r->data, ">#", 2) != 0)
            reader_advance(r);
        reader_advance_n(r, 2); // >#
    }
    else
    {
        skip_to_eol(r);
    }
}


void consume_whitespace(Reader *r, b32 consume_newlines)
{
    for (;;)
    {
        switch (r->data[0])
        {
        case '\n':
        case '\r':
            if (!consume_newlines) break;
            
        case ' ':
        case '\t':
        case '\f':
        case '\v':
            reader_advance(r);
            continue;

        case '#': // comment
            skip_comment(r);
            continue;
        }
        break;
    }
}

b32 expect_eq(Reader *r)
{
    if (r->data[0] == '=')
    {
        reader_advance(r);
        consume_whitespace(r, false);
        return true;
    }
    return false;
}

String read_line(Reader *r)
{
    String ret;
    ret.start = r->data;
    skip_to_eol(r);
    ret.len = r->data - ret.start;
    
    consume_whitespace(r, true);

    return ret;
}

String read_ident(Reader *r)
{
    String ret;
    ret.start = r->data;
    if (!(gb_char_is_alpha(r->data[0]) || r->data[0] == '_'))
    {
        reader_error(r, "'%c' is not a valid identifier character\n", r->data[0]);
        gb_exit(1);
    }
    
    while (gb_char_is_alphanumeric(r->data[0]) || r->data[0] == '-' || r->data[0] == '_')
        reader_advance(r);

    ret.len = r->data - ret.start;
    consume_whitespace(r, false);
    
    return ret;
}

String read_header(Reader *r)
{
    reader_advance_n(r, 3); // ::/
    return read_line(r);
}

b32 is_header(Reader *r)
{
    return gb_strncmp(r->data, "::/", 3) == 0;
}

String read_string(Reader *r)
{
    require_c(r, '"', "start of string");
    
    String ret;
    ret.start = r->data;

    while (r->data[0] != '"')
    {
        if (r->data[0] == '\\')
            reader_advance(r);
        reader_advance(r);
    }

    ret.len = r->data - ret.start;
    require_c(r, '"', "end of string");
    consume_whitespace(r, false);
    
    return ret;
}

String read_path(Reader *r)
{
    String ret;

    if (r->data[0] == '"')
    {
        ret = read_string(r);
        return ret;
    }
    
    ret.start = r->data;
    while (!gb_char_is_space(r->data[0]) && r->data[0] != ':' && r->data[0] != ']' && r->data[0] !='}')
    {
        if (r->data[0] == '\\')
            reader_advance(r);
        reader_advance(r);
    }

    ret.len = r->data - ret.start;
    consume_whitespace(r, false);

    return ret;
}

void read_list(Reader *r, gbArray(String) *list)
{
    require_c(r, '[', "start of list");
    consume_whitespace(r, true);
    
    if (!*list) gb_array_init(*list, r->alloc);
    while (r->data[0] != ']')
    {
        gb_array_append(*list, read_path(r));
        consume_whitespace(r, true);
    }

    require_c(r, ']', "end of list");
}

void *read_map_value(Reader *r)
{
    if (r->data[0] == '[') // list
    {
        gbArray(String) list;
        gb_array_init(list, r->alloc);
        read_list(r, &list);
        return (void *)list;
    }
    else
    {
        String *ret = gb_alloc_item(r->alloc, String);
        *ret = read_path(r);
        return (void *)ret;
    }
}

void read_map(Reader *r, map_t *map)
{
    require_c(r, '{', "start of map");

    consume_whitespace(r, true);

    if (!*map) *map = hashmap_new(r->alloc);
    
    while (r->data[0] != '}')
    {
        String key = read_path(r);
        if (r->data[0] != ':')
            reader_error(r, "Expected ':' after key \"%.*s\" in map, got '%c'\n", LIT(key), r->data[0]);
        reader_advance(r); // :
        void *value = read_map_value(r);
        hashmap_put(*map, key, value);
        consume_whitespace(r, true);
    }
    
    require_c(r, '}', "end of map");
}

void read_general_config(Reader *r)
{
    while (!is_header(r) && r->data[0] != 0)
    {
        String label = read_ident(r);
        if (!expect_eq(r))
            reader_error(r, "Expected '=' after label, got '%c'\n", r->data[0]);
    
        if (cstring_cmp(label, "directory") == 0)
            r->conf->directory = read_path(r);
        else if (cstring_cmp(label, "files") == 0)
            read_list(r, &r->conf->files);
        else if (cstring_cmp(label, "output-directory") == 0)
            r->conf->out_directory = read_path(r);
        else if (cstring_cmp(label, "output-file") == 0)
            r->conf->out_file = read_path(r);
        else
            reader_error(r, "Invalid label \"%.*s\" in ::/general\n", LIT(label));
        consume_whitespace(r, true);
    }
}

void read_pp_config(Reader *r)
{
    while (!is_header(r) && r->data[0] != 0)
    {
        String label = read_ident(r);
        if (!expect_eq(r))
            reader_error(r, "Expected '=' after label, got '%c'\n", r->data[0]);
    
        if (cstring_cmp(label, "include-dirs") == 0)
            read_list(r, &r->conf->pp_conf.include_dirs);
        else if (cstring_cmp(label, "symbols") == 0)
            read_map(r, &r->conf->pp_conf.custom_symbols);
        else if (cstring_cmp(label, "whitelist") == 0)
            r->conf->pp_conf.whitelist = read_path(r);
        else if (cstring_cmp(label, "pre-includes") == 0)
            read_map(r, &r->conf->pp_conf.pre_includes);
        else
            reader_error(r, "Invalid label \"%.*s\" in ::/preprocessor\n", LIT(label));
        consume_whitespace(r, true);
    }
}

Case read_case(Reader *r)
{
    String name = read_ident(r);

    if (cstring_cmp(name, "Ada_Case") == 0)
        return Case_ADA;
    else if (cstring_cmp(name, "snake_case") == 0)
        return Case_SNAKE;
    else if (cstring_cmp(name, "SCREAMING_SNAKE_CASE") == 0)
        return Case_SCREAMING_SNAKE;
    else if (cstring_cmp(name, "NONE") == 0)
        return Case_NONE;
    else
        reader_error(r, "\"%.*s\" is not a valid case type. Must be one of Ada_Case, snake_case, SCREAMING_SNAKE_CASE, or NONE\n", LIT(name));
    return Case_NONE;
}

BindOrdering read_ordering(Reader *r)
{
    String name = read_ident(r);

    if (cstring_cmp(name, "SORTED") == 0)
        return Ordering_Sorted;
    else if (cstring_cmp(name, "SOURCE") == 0)
        return Ordering_Source;
    else
        reader_error(r, "\"%.*s\" is not a valid ordering. Must be one of SORTED or SOURCE\n", LIT(name));
    return 0;
}

void read_bind_config(Reader *r)
{
    while (!is_header(r) && r->data[0] != 0)
    {
        String label = read_ident(r);
        if (!expect_eq(r))
            reader_error(r, "Expected '=' after label, got '%c'\n", r->data[0]);
        
        if (cstring_cmp(label, "package") == 0)
            r->conf->bind_conf.package_name = read_ident(r);
        else if (cstring_cmp(label, "lib") == 0)
            r->conf->bind_conf.lib_name = read_ident(r);
        else if (cstring_cmp(label, "prefix") == 0)
            r->conf->bind_conf.type_prefix = r->conf->bind_conf.var_prefix
                = r->conf->bind_conf.proc_prefix = r->conf->bind_conf.const_prefix = read_path(r);
        else if (cstring_cmp(label, "type-prefix") == 0)
            r->conf->bind_conf.type_prefix = read_path(r);
        else if (cstring_cmp(label, "var-prefix") == 0)
            r->conf->bind_conf.var_prefix = read_path(r);
        else if (cstring_cmp(label, "proc-prefix") == 0)
            r->conf->bind_conf.proc_prefix = read_path(r);
        else if (cstring_cmp(label, "const-prefix") == 0)
            r->conf->bind_conf.const_prefix = read_path(r);
        else if (cstring_cmp(label, "type-case") == 0)
            r->conf->bind_conf.type_case = read_case(r);
        else if (cstring_cmp(label, "var-case") == 0)
            r->conf->bind_conf.var_case = read_case(r);
        else if (cstring_cmp(label, "proc-case") == 0)
            r->conf->bind_conf.proc_case = read_case(r);
        else if (cstring_cmp(label, "const-case") == 0)
            r->conf->bind_conf.const_case = read_case(r);
        else if (cstring_cmp(label, "ordering") == 0)
            r->conf->bind_conf.ordering = read_ordering(r);
        else if (cstring_cmp(label, "custom-ordering") == 0)
            read_list(r, &r->conf->bind_conf.custom_ordering);
        else if (cstring_cmp(label, "custom-types") == 0)
            read_map(r, &r->conf->bind_conf.custom_types);
        else
            reader_error(r, "Invalid label \"%.*s\" in ::/bind\n", LIT(label));
        consume_whitespace(r, true);
    }
}

void read_config(Reader *r)
{
    while (r->data[0] != 0)
    {
        consume_whitespace(r, true);
        if (!is_header(r))
        {
            gb_printf("Expected section header, got \"%.*s\"\n", LIT(read_line(r)));
            gb_exit(1);
        }
    
        String header = read_header(r);
        if (cstring_cmp(header, "general") == 0)
            read_general_config(r);
        else if (cstring_cmp(header, "preprocess") == 0)
            read_pp_config(r);
        else if (cstring_cmp(header, "bind") == 0)
            read_bind_config(r);
        else
            reader_error(r, "Invalid section header \"%.*s\"\n", LIT(header));
    }
}

Config *load_config(char *file, gbAllocator a)
{
    gbFileContents fc = gb_file_read_contents(a, true, file);
    if (!fc.data)
    {
        gb_printf_err("Could not load config file \"%s\"\n", file);
        gb_exit(1);
    }
    
    Reader reader = {0};
    reader.data = fc.data;
    reader.file = file;
    reader.line = 1;
    reader.alloc = a;
    reader.conf = gb_alloc_item(a, Config);
    
    read_config(&reader);
    
    return reader.conf;
}

void update_config(Config *conf, char *file, gbAllocator a)
{
    gbFileContents fc = gb_file_read_contents(a, true, file);
    if (!fc.data)
    {
        gb_printf_err("Could not load config file \"%s\"\n", file);
        gb_exit(1);
    }
    
    Reader reader = {0};
    reader.data = fc.data;
    reader.file = file;
    reader.line = 1;
    reader.alloc = a;
    reader.conf = conf;
    
    read_config(&reader);
}

void print_list(gbArray(String) list)
{
    gb_printf("[");
    for (int i = 0; i < gb_array_count(list); i++)
    {
        gb_printf("\"%.*s\"", LIT(list[i]));
        if (i+1 < gb_array_count(list))
            gb_printf(" ");
    }
    gb_printf("]");
}

int print_string_entry(String key, any_t data)
{
    String entry = *(String *)data;
    gb_printf("%.*s:%.*s ", LIT(key), LIT(entry));
    return MAP_OK;
}

int print_list_entry(String key, any_t data)
{
    gbArray(String) entry = (gbArray(String))data;
    gb_printf("%.*s:", LIT(key));
    print_list(entry);
    gb_printf(" ");
    return MAP_OK;
}

void print_map(map_t map, PFentry func)
{
    gb_printf("{ ");
    hashmap_iterate_entries(map, func);
    gb_printf("}");
}

char *get_case_string(Case c)
{
    switch (c)
    {
    case Case_NONE:            return "NONE";
    case Case_ADA:             return "Ada_Case";
    case Case_SNAKE:           return "snake_case";
    case Case_SCREAMING_SNAKE: return "SCREAMING_SNAKE";
    default: return "INVALID";
    }
}

void print_config(Config *conf)
{
    gb_printf("===== CONFIG =====\n\n");
    gb_printf("::/general\n");
    if (conf->files)
    {
        gb_printf("files = ");
        print_list(conf->files);
        gb_printf("\n");
    }
    if (conf->out_file.len)      gb_printf("output-file = \"%.*s\"\n", LIT(conf->out_file));
    if (conf->directory.len)     gb_printf("directory = \"%.*s\"\n", LIT(conf->directory));
    if (conf->out_directory.len) gb_printf("output-directory = \"%.*s\"\n", LIT(conf->out_directory));

    PreprocessorConfig pp = conf->pp_conf;
    gb_printf("\n::/preprocess\n");
    if (pp.include_dirs)
    {
        gb_printf("include-dirs = ");
        print_list(pp.include_dirs);
        gb_printf("\n");
    }
    if (pp.custom_symbols)
    {
        gb_printf("custom-symbols = ");
        print_map(pp.custom_symbols, print_string_entry);
        gb_printf("\n");
    }
    if (pp.whitelist.len) gb_printf("whitelist = \"%.*s\"\n", LIT(pp.whitelist));
    if (pp.pre_includes)
    {
        gb_printf("pre-includes = ");
        print_map(pp.pre_includes, print_list_entry);
        gb_printf("\n");
    }

    BindConfig bind = conf->bind_conf;
    gb_printf("\n::/bind\n");
    if (bind.package_name.start) gb_printf("package = \"%.*s\"\n", LIT(bind.package_name));
    if (bind.lib_name.start)     gb_printf("lib = \"%.*s\"\n", LIT(bind.lib_name));
    
    if (bind.type_prefix.start)  gb_printf("type-prefix  = \"%.*s\"\n", LIT(bind.type_prefix));
    if (bind.var_prefix.start)   gb_printf("var-prefix   = \"%.*s\"\n", LIT(bind.var_prefix));
    if (bind.proc_prefix.start)  gb_printf("proc-prefix  = \"%.*s\"\n", LIT(bind.proc_prefix));
    if (bind.const_prefix.start) gb_printf("const-prefix = \"%.*s\"\n", LIT(bind.const_prefix));
    
    if (bind.type_case)   gb_printf("type-case  = %s\n", get_case_string(bind.type_case));
    if (bind.var_case)    gb_printf("var-case   = %s\n", get_case_string(bind.var_case));
    if (bind.proc_case)   gb_printf("proc-case  = %s\n", get_case_string(bind.proc_case));
    if (bind.const_case)  gb_printf("const-case = %s\n", get_case_string(bind.const_case));

    if (bind.ordering) gb_printf("ordering = %s\n", bind.ordering == Ordering_Sorted ? "SORTED" : "SOURCE");
    
    if (bind.custom_ordering)
    {
        gb_printf("custom-ordering = ");
        print_list(bind.custom_ordering);
        gb_printf("\n");
    }
    if (bind.custom_types)
    {
        gb_printf("custom-types = ");
        print_map(bind.custom_types, print_string_entry);
        gb_printf("\n");
    }
    gb_printf("\n==================\n");
}
