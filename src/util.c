#include "util.h"

#include "error.h"

#ifdef GB_SYSTEM_WINDOWS
# include "vs_find.h"
# include <stdlib.h>
#endif

Token_Run alloc_token_run(Token *tokens, int count)
{
    int len = 0;
    int line = tokens[0].loc.line;
    int column = tokens[0].loc.column;
    for (int i = 0; i < count; i++)
    {
        if (tokens[i].loc.line > line)
        {
            len += tokens[i].loc.column+1;
            line = tokens[i].loc.line;
            column = tokens[i].loc.column;
        }
        len += tokens[i].str.len;
        column += tokens[i].str.len;
    }

    String str = {gb_alloc(gb_heap_allocator(), len), len};

    char *curr = str.start;

    line = tokens[0].loc.line;
    column = tokens[0].loc.column;
    for (int i = 0; i < count; i++)
    {
        if (tokens[i].loc.line > line)
        {
            *curr++ = '\n';
            gb_memset(curr, ' ', tokens[i].loc.column);
            curr += tokens[i].loc.column;
            line = tokens[i].loc.line;
            column = tokens[i].loc.column;
        }
        gb_memcopy(curr, tokens[i].str.start, tokens[i].str.len);
        tokens[i].str.start = curr;
        column += tokens[i].str.len;
        curr += tokens[i].str.len;
    }

    Token_Run run = {tokens, tokens, tokens+gb_array_count(tokens)-1};
    return run;
}

void create_path_to_file(char const *filename)
{
    String curr_dir = {(char *)filename, 0};
    if (curr_dir.start[0] == GB_PATH_SEPARATOR
        || (curr_dir.start[0] == '.' && curr_dir.start[1] == GB_PATH_SEPARATOR))
        filename++;

    char const *d = 0;
    while (filename && (d = gb_char_first_occurence(filename, GB_PATH_SEPARATOR)) != 0)
    {
        curr_dir.len = d - curr_dir.start;

        char *temp = make_cstring(gb_heap_allocator(), curr_dir);
        gb_dir_create(temp);
        gb_free(gb_heap_allocator(), temp);

        filename = d+1;
    }
}

char *date_string(u64 time)
{
    int month_days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    char const *month_names[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    int years = time/31557600000000;
    int num_days = time/86400000000;

    int leap_years;
    if (years > 264)
        leap_years = 64 + (years-266)/4;
    else
        leap_years = (years+1)/4;
    if ((years+1)%4 == 0) month_days[2]++; // current year is leap year

    int days_in_year = num_days - (years*365+leap_years) + 1;
    int days_in_month = days_in_year;
    int month = 0;
    for (int i = 0; i < 12; i++)
    {
        if (days_in_month < month_days[i])
        {
            month = i;
            break;
        }
        days_in_month = days_in_month - month_days[i];
    }

    char *ret = gb_alloc(gb_heap_allocator(), 14);
    gb_snprintf(ret, 14, "\"%.3s %.2d %.4d\"", month_names[month], days_in_month, 1601+years);

    return ret;
}

// TODO: Time is currently in UTC; Change to local time
char *time_string(u64 time)
{
    u64 time_in_day = time%86400000000;
    int hours   =  time_in_day            /3600000000;
    int minutes = (time_in_day%3600000000)/60000000;
    int seconds = (time_in_day%60000000)  /1000000;

    char *ret = gb_alloc(gb_heap_allocator(), 11);
    gb_snprintf(ret, 11, "\"%02d:%02d:%02d\"", hours, minutes, seconds);
    return ret;
}

String convert_case(String str, Case case_type, gbAllocator allocator)
{
    if (!str.start || case_type == Case_NONE)
        return str;

    String word_matches[20];

    int word_count = 0;
    int length = 0;
    int first_char = 1;
    int i = 0;

    word_matches[word_count].start = str.start;
    if (str.start[i] == '_')
    {
        while (str.start[i] == '_')
            i++;
        word_matches[word_count].len = i-1;
        length += word_matches[word_count].len + 1;
        word_count++;
        word_matches[word_count].start = str.start+i;
    }

    while (i < str.len)
    {
        if (gb_is_between(str.start[i], 'a', 'z'))
        {
            // TODO: Decide between (utf8_func / foo_func8) and (utf_8_func / foo_func_8)
            while (i < str.len && !gb_is_between(str.start[i], 'A', 'Z') /*&& !isdigit(str.start[i])*/ && str.start[i] != '_')
                i++;
            word_matches[word_count].len = (str.start+i) - word_matches[word_count].start;
            length += word_matches[word_count].len + 1;
            word_count++;
            while (i < str.len && str.start[i] == '_')
                i++;
            word_matches[word_count].start = str.start+i;
        }
        else if (gb_is_between(str.start[i], 'A', 'Z') && !first_char)
        {
            if (i+1 < str.len && str.start[i+1] == '_')
            {
                word_matches[word_count].len = (str.start+i) - word_matches[word_count].start + 1;
                length += word_matches[word_count].len + 1;
                word_count++;
                do
                {
                    i++;
                } while (str.start[i] == '_');
                word_matches[word_count].start = str.start+i;
            }
            else if (i+1 < str.len && gb_is_between(str.start[i+1], 'a', 'z'))
            {
                word_matches[word_count].len = (str.start+i) - word_matches[word_count].start;
                length += word_matches[word_count].len + 1;
                word_count++;
                while (str.start[i] == '_')
                    i++;
                word_matches[word_count].start = str.start + i;
            }
        }
        if (i < str.len)
            i++;
        if (first_char)
            first_char = 0;
    }

    if (word_matches[word_count].start != str.start+i)
    {
        word_matches[word_count].len = (str.start+i) - word_matches[word_count].start;
        length += word_matches[word_count].len + 1;
        word_count++;
    }

    if (length == 0)
        return (String){0, 0};

    String result;
    result.start = (char *)gb_alloc(allocator, length-1);
    result.len = length-1;
    char *start;
    int pos;
    for (i = 0; i < word_count; i++)
    {
        start = word_matches[i].start;
        for (pos = 0; pos < word_matches[i].len; pos++)
        {
            // TODO: Decide on best case conversion from SCREAMING_SNAKE_CASE
            if (case_type == Case_SCREAMING_SNAKE)
                result.start[pos] = gb_char_to_upper(start[pos]);
            else if(case_type == Case_SNAKE /*|| is_screaming_snake*/)
                result.start[pos] = gb_char_to_lower(start[pos]);
            else
                result.start[pos] = start[pos];
        }
        if (case_type == Case_ADA)
            result.start[0] = gb_char_to_upper(result.start[0]);

        result.start += word_matches[i].len + 1;
        if (i != word_count-1)
            *(result.start-1) = '_';
    }

    result.start -= length;

    return result;
}

String to_snake_case(String str, gbAllocator allocator)
{
    return convert_case(str, Case_SNAKE, allocator);
}
String to_ada_case(String str, gbAllocator allocator)
{
    return convert_case(str, Case_ADA, allocator);
}
String to_screaming_snake_case(String str, gbAllocator allocator)
{
    return convert_case(str, Case_SCREAMING_SNAKE, allocator);
}

// TODO(@Completeness): Not yet implemented
String to_camel_case(String str, gbAllocator allocator)
{
    return convert_case(str, Case_CAMEL, allocator);
}

void init_rename_map(map_t rename_map, gbAllocator allocator)
{
    // map_t rename_map = hashmap_new(allocator);
    char *reserved_renames[] =
    {
        /* "type",    "kind", */
        "string",  "str",
        "cstring", "cstr",
        "import",  "import_",
        "using",   "using_",
        "map",     "map_",
        "proc",    "procedure",
        "context", "context_",
        "any",     "any_",
        "in",      "in_",
        "when",    "when_",
        "package", "package_",
        "macro",   "macro_",
    };

    int count = sizeof(reserved_renames)/(sizeof(reserved_renames[0]));
    String *alloc;
    for (int i = 0; i < count/2; i++)
    {
        alloc = gb_alloc_item(allocator, String);
        alloc->start = gb_alloc(allocator, gb_strlen(reserved_renames[i*2+1]));
        gb_strcpy(alloc->start, reserved_renames[i*2+1]);
        alloc->len = gb_strlen(reserved_renames[i*2+1]);
        hashmap_put(rename_map, make_string(reserved_renames[i*2]), alloc);
    }
}

String float_type_str(Node *type)
{
    gbArray(Token) specs = type->FloatType.specifiers;

    int size = -1;
    int shift = 0;

    for (int i = 0; i < gb_array_count(specs); i++)
    {
        switch (specs[i].kind)
        {
            case Token_float:  size = 32; break;
            case Token_double: size = 64; break;

            case Token_long:   shift++;   break;
            default:
            error(specs[i], "Invalid type specifier '%.*s' in floating point type", LIT(TokenKind_Strings[specs[i].kind]));
        }
    }

    if (size == -1 || shift > 1 || (size == 32 && shift))
        error(specs[0], "Invalid floating point type");

    int len = 3 + 5 + (size == 64);
    String ret;
    ret.start = gb_alloc(gb_heap_allocator(), len+1);
    gb_snprintf(ret.start, len, "_c.%s", size==32 ? "float" : "double");
    ret.len = len;

    return ret;
}

String integer_type_str(Node *type)
{
    gbArray(Token) specs = type->IntegerType.specifiers;

    int size = 0;
    int is_signed = -1;
    b32 is_int = true;
    int lngs  = 0;
    int shrts = 0;
    for (int i = 0; i < gb_array_count(specs); i++)
    {
        switch (specs[i].kind)
        {
            case Token_char:
            is_int = false;
            if (is_signed == -1)
                is_signed = 0;
            break;

            case Token_signed:   is_signed = 1; break;
            case Token_unsigned: is_signed = 0; break;
            case Token_short:    shrts++;       break;
            case Token_long:     lngs++;        break;
            case Token__int8:    size = 8;      break;
            case Token__int16:   size = 16;     break;
            case Token__int32:   size = 32;     break;
            case Token__int64:   size = 64;     break;
            case Token_int:                     break;

            default:
            error(specs[i], "Invalid type specifier '%.*s' in integer type", LIT(TokenKind_Strings[specs[i].kind]));
        }
    }

    if ((shrts > 0 && lngs > 0)
        || (lngs > 2 || shrts > 1)
        || ((lngs > 0 || shrts > 0) && (size > 0 || !is_int)) )
        syntax_error(specs[0], "Invalid type sepcifiers in integer type\n");

    String ret = {0};
    if (!lngs && !shrts && !size)
    {
        int len = !is_signed + (is_signed == 1 && !is_int) + (3 + !is_int);
        ret.start = gb_alloc(gb_heap_allocator(), 3+len+1);
        ret.len = 3+len;

        char *sign = !is_signed ? "u" : (is_signed == 1 && !is_int) ? "s" : "";
        char *type = is_int ? "int" : "char";
        gb_snprintf(ret.start, 3+len, "_c.%s%s", sign, type);
    }
    else if (lngs || shrts)
    {
        int len = !is_signed + (lngs * 4) + (shrts * 5);
        ret.start = gb_alloc(gb_heap_allocator(), 3+len+1);
        ret.len = 3+len;

        char *tmp = ret.start;
        gb_strcpy(tmp, "_c.");
        tmp += 3;
        if (!is_signed)
            *tmp++ = 'u';
        for (int i = 0; i < lngs; i++, tmp += 4)
            gb_strcpy(tmp, "long");
        for (int i = 0; i < shrts; i++, tmp += 5)
            gb_strcpy(tmp, "short");
    }
    else if (size)
    {
        int len = 1 + (1+(size > 8));
        ret.start = gb_alloc(gb_heap_allocator(), len+1);
        ret.len = len;

        gb_snprintf(ret.start, len, "%c%d", is_signed?'i':'u', size);
    }

    return ret;
}

String convert_type(Node *type, map_t rename_map, BindConfig *conf, gbAllocator allocator)
{
    char *result = 0;
    if (type->kind == NodeKind_Ident)
    {
        String ident = type->Ident.token.str;
        /*
        if (cstring_cmp(ident, "QWORD") == 0 || cstring_cmp(ident, "uint64_t") == 0)
            result = "u64";
        else if (cstring_cmp(ident, "DWORD") == 0 || cstring_cmp(ident, "uint32_t") == 0)
            result = "u32";

        else if (cstring_cmp(ident, "wchar_t") == 0 || cstring_cmp(ident, "WORD") == 0 || cstring_cmp(ident, "uint16_t") == 0)
            result = "u16";
        */
        if (cstring_cmp(ident, "wchar_t") == 0)
            result = "_c.wchar_t";
        else if (cstring_cmp(ident, "size_t") == 0)
            result = "_c.size_t";
        else if (cstring_cmp(ident, "ssize_t") == 0)
            result = "_c.ssize_t";
        else if (cstring_cmp(ident, "ptrdiff_t") == 0)
            result = "_c.ptrdiff_t";
        else if (cstring_cmp(ident, "uintptr_t") == 0)
            result = "_c.uintptr_t";
        else if (cstring_cmp(ident, "intptr_t") == 0)
            result = "_c.intptr_t";
        /*
        else if (cstring_cmp(ident, "PWORD") == 0)
            result = "^u16";
        */
        else if (cstring_cmp(ident, "_Bool") == 0)
            result = "_c.bool";
        else
        {
            String renamed = rename_ident(ident, RENAME_TYPE, true, rename_map, conf, allocator);
            return renamed;
        }
    }
    else if (type->kind == NodeKind_IntegerType)
    {
        return integer_type_str(type);
    }
    else if (type->kind == NodeKind_FloatType)
    {
        return float_type_str(type);
    }
    else if (type->kind == NodeKind_StructType
             || type->kind == NodeKind_UnionType
             || type->kind == NodeKind_EnumType)
    {
        String renamed = {0};
        if (type->StructType.name)
            renamed = rename_ident(type->StructType.name->Ident.token.str, RENAME_TYPE, true, rename_map, conf, allocator);
        return renamed;
    }

    String ret;
    ret.start = (char *)gb_alloc(allocator, gb_strlen(result));
    ret.len = gb_strlen(result);
    gb_strcpy(ret.start, result);
    return ret;
}

String remove_prefix(String str, String prefix, gbAllocator allocator)
{
    if (!prefix.start || !str.start)
        return str;
    int underscores = 0;
    while (str.start[underscores] == '_' && underscores < str.len)
        underscores++;

    if (prefix.len >= str.len-underscores)
        return str;

    int i = 0;
    // while (i < str.len && i < prefix.len && gb_char_to_lower(str.start[underscores+i]) == gb_char_to_lower(prefix.start[i]))
    while (i < str.len && i < prefix.len && str.start[underscores+i] == prefix.start[i]) // NOTE(@Design): Should this be case sensitive?
        i++;
    if (i == prefix.len)
    {
        /*
        while (str.start[i] == '_')
            i++;
        */
        String ret;
        ret.start = gb_alloc(allocator, str.len-i);
        ret.len = str.len-i;
        for (int j = 0; j < underscores; j++)
            ret.start[j] = '_';
        for (int j = 0; j < str.len - (underscores+i); j++)
            ret.start[underscores+j] = str.start[(i+underscores)+j];
        return ret;
    }
    return str;
}

String rename_ident(String orig, Rename_Kind r, b32 do_remove_prefix, map_t rename_map, BindConfig *conf, gbAllocator allocator)
{
    String *ret;
    if (hashmap_get(rename_map, orig, (void **)&ret) == 0)
        return *ret;

    String unprefixed = orig;
    if (do_remove_prefix)
    {
        String prefix = {0};
        switch (r)
        {
            case RENAME_TYPE:  prefix = conf->type_prefix; break;
            case RENAME_VAR:   prefix = conf->var_prefix; break;
            case RENAME_PROC:  prefix = conf->proc_prefix; break;
            case RENAME_CONST: prefix = conf->const_prefix; break;
        }
        if (prefix.start)
            unprefixed = remove_prefix(orig, prefix, allocator);
    }

    String cased = {0};
    switch (r)
    {
        case RENAME_TYPE:  cased = convert_case(unprefixed, conf->type_case, allocator);  break;
        case RENAME_VAR:   cased = convert_case(unprefixed, conf->var_case, allocator);   break;
        case RENAME_PROC:  cased = convert_case(unprefixed, conf->proc_case, allocator);  break;
        case RENAME_CONST: cased = convert_case(unprefixed, conf->const_case, allocator); break;
    }

    if (hashmap_get(rename_map, cased, (void **)&ret) == 0)
        return *ret;

    return cased;
}

char *repeat_char(char c, int count, gbAllocator allocator)
{
    if (count < 0)
        return 0;
    char *repeated = (char *)gb_alloc(allocator, count+1);
    if (count < 0) count = 0;
    for (int i = 0; i < count; i++)
        repeated[i] = c;
    return repeated;
}

b32 is_valid_ident(String ident)
{
    if (ident.len == 0) return false;
    if (!gb_char_is_alpha(ident.start[0]) && ident.start[0] != '_') return false;
    for (int i = 1; i < ident.len; i++)
    {
        if (!gb_char_is_alphanumeric(ident.start[i]) && ident.start[i] != '_')
        {
            if (ident.start[i] == ' ' || ident.start[i] == '(')
                return true;
            else
                return false;
        }
    }

    return true;
}


u64 str_to_int_base(String str, u64 base)
{
    u64 result = 0;
    for (int i = 0; i < str.len; i++)
    {
        u64 n;
        if (base == 16 && gb_char_is_hex_digit(str.start[i]))
            n = gb_hex_digit_to_int(str.start[i]);
        else if (base == 8 && gb_is_between(str.start[i], '0', '7'))
            n = gb_digit_to_int(str.start[i]);
        else if (base == 10 && gb_char_is_digit(str.start[i]))
            n = gb_digit_to_int(str.start[i]);
        else
            break;
        result *= base;
        result += n;
    }
    return result;
}

u64 str_to_int(String str)
{
    int i = 0;
    u64 base = 10;
    if (csubstring_cmp(str, "0x") == 0 || csubstring_cmp(str, "0X") == 0)
    {
        base = 16;
        str.start += 2;
        str.len -= 2;
    }
    else if (str.start[0] == '0' && str.len > 1)
    {
        base = 8;
        str.start += 1;
        str.len -= 1;
    }

    return str_to_int_base(str, base);
}

#ifdef GB_SYSTEM_WINDOWS
System_Directories get_system_includes(gbAllocator a)
{
    Find_Result res = find_visual_studio_and_windows_sdk();

    System_Directories system = {0};
    gb_array_init(system.include, a);
    gb_array_init(system.lib, a);

    if (res.windows_sdk_version != 0)
    {
        gb_array_append(system.include, make_string(gb_alloc_str(a, (char const *)gb_ucs2_to_utf8_buf(res.windows_sdk_ucrt_include_path))));
        gb_array_append(system.include, make_string(gb_alloc_str(a, (char const *)gb_ucs2_to_utf8_buf(res.windows_sdk_shared_include_path))));
        gb_array_append(system.include, make_string(gb_alloc_str(a, (char const *)gb_ucs2_to_utf8_buf(res.windows_sdk_um_include_path))));

        gb_array_append(system.lib, make_string_alloc(a, (char *)gb_ucs2_to_utf8_buf(res.windows_sdk_um_lib_path)));
        gb_array_append(system.lib, make_string_alloc(a, (char *)gb_ucs2_to_utf8_buf(res.windows_sdk_ucrt_lib_path)));
    }

    if (res.vs_include_path)
        gb_array_append(system.include, make_string(gb_alloc_str(a, (char const *)gb_ucs2_to_utf8_buf(res.vs_include_path))));

    for (int i = 0; i < gb_array_count(system.include); i++)
        gb_printf("FOUND SYSTEM INCLUDE DIRECTORY: %.*s\n", LIT(system.include[i]));
    for (int i = 0; i < gb_array_count(system.lib); i++)
        gb_printf("FOUND SYSTEM LIB DIRECTORY: %.*s\n", LIT(system.lib[i]));
    return system;
}
#else
void get_gcc_includes (System_Directories *system, gbAllocator a)
{
    gbArray(char *) platforms;
    gbArray(char *) versions;
    gbArray(char *) entries;

    gb_array_init(versions, a);
    if (!gb_dir_contents("/usr/lib/gcc/x86_64-pc-linux-gnu", &versions, false))
        return;
    for (int j = 0 ; j < gb_array_count(versions); j++)
    {
        gb_array_init(entries, a);
        if (!gb_dir_contents(versions[j], &entries, false))
            continue;
        for (int k = 0; k < gb_array_count(entries); k++)
        {
            String path = make_string(entries[k]);
            if (cstring_cmp(string_slice(path, path.len-7, -1), "include") == 0)
                gb_array_append(system->includes, alloc_string(path));
            else if (cstring_cmp(string_slice(path, path.len-13, -1), "include-fixed") == 0)
                gb_array_append(system->includes, alloc_string(path));
        }
        gb_array_free(entries);
    }
    gb_array_free(versions);
}

void get_clang_includes(System_Directories *system, gbAllocator a)
{
    gbArray(char *) versions;
    gbArray(char *) entries;

    gb_array_init(versions, a);
    gb_dir_contents("/usr/lib/clang", &versions, false);
    for (int j = 0 ; j < gb_array_count(versions); j++)
    {
        gb_array_init(entries, a);
        if (!gb_dir_contents(versions[j], &entries, false))
            continue;
        for (int k = 0; k < gb_array_count(entries); k++)
        {
            String path = make_string(entries[k]);
            if (cstring_cmp(string_slice(path, path.len-7, -1), "include") == 0)
                gb_array_append(system->include, alloc_string(path));
        }
        gb_array_free(entries);
    }
    gb_array_free(versions);
}

System_Directories get_system_includes(gbAllocator a)
{
    System_Directories system = {0};
    gb_array_init(system.include, a);
    gb_array_init(system.lib, a);

    if (gb_file_exists("/usr/include"))
        gb_array_append(system.include, make_string_alloc(a, "/usr/include"));
    if (gb_file_exists("/usr/local/include"))
        gb_array_append(system.include, make_string_alloc(a, "/usr/local/include"));

    if (gb_file_exists("/usr/lib/gcc/x86_64-pc-linux-gnu"))
        get_gcc_includes(&system, a);
    else if (gb_file_exists("/usr/lib/clang"))
        get_clang_includes(&system, a);

    gb_array_append(system.lib, make_string_alloc(a, "/usr/lib"));

    for (int i = 0; i < gb_array_count(system.include); i++)
        gb_printf("FOUND SYSTEM INCLUDE DIRECTORY: %.*s\n", LIT(system.include[i]));
    for (int i = 0; i < gb_array_count(system.lib); i++)
        gb_printf("FOUND SYSTEM LIB DIRECTORY: %.*s\n", LIT(system.lib[i]));

    return system;
}
#endif

char *find_lib_path(System_Directories system_dirs, String lib)
{

    b32 found_sep = false;
    for (int i = 0; i < lib.len; i++)
    {
        if (lib.start[i] == '/' || lib.start[i] == '\\')
        {
            found_sep = true;
            break;
        }
    }

    char path[512];
    if (found_sep)
    {
        gb_snprintf(path, 512, "%.*s", LIT(lib));
        if (gb_file_exists(path))
            return gb_alloc_str(gb_heap_allocator(), path);

        gb_printf_err("ERROR: Could not find local library \"%s\"\n", path);
        return 0;
    }

    for (int i = 0; i < gb_array_count(system_dirs.lib); i++)
    {
        gb_snprintf(path, 512, "%.*s%c%.*s", LIT(system_dirs.lib[i]), GB_PATH_SEPARATOR, LIT(lib));
        if (gb_file_exists(path))
            return gb_alloc_str(gb_heap_allocator(), path);
    }
    return 0;
}

gbArray(Lib) get_library_info(System_Directories system_dirs, gbArray(String) libraries)
{
    if (!libraries) return 0;
    
    gbArray(Lib) libs;
    gb_array_init(libs, gb_heap_allocator());
    for (int i = 0; i < gb_array_count(libraries); i++)
    {
        char *path = find_lib_path(system_dirs, libraries[i]);
        if(path)
        {
            gb_printf("GETTING SYMBOLS FROM \"%s\"\n", path);
            gb_array_append(libs, get_coff_symbols(path));
        }
    }
    return libs;
}
