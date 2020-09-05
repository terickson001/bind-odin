#include "define.h"
#include "util.h"

#include "signal.h"

GB_TABLE_DEFINE(Define_Map, defines_, Define);

void add_define(Define_Map **defines, String name, Token_Run value, gbArray(Token_Run) params, isize line, String file)
{
    Define *old = get_define(*defines, name);
    if (old && old->in_use)
    {
        gb_free(gb_heap_allocator(), old->file.start);
        if (old->params)
            gb_array_free(old->params);
    }
    Define define = {1, alloc_string(name), value, params, alloc_string(file), line};
    defines_set(*defines, gb_crc64(name.start, name.len), define);
}

void add_fake_define(Define_Map **defines, String name)
{
    Define define = {0};
    defines_set(*defines, gb_crc64(name.start, name.len), define);
}

void remove_define(Define_Map **defines, String name)
{
    Define *old = get_define(*defines, name);
    if (old && old->in_use)
        gb_free(gb_heap_allocator(), old->file.start);
    Define define = {0};
    defines_set(*defines, gb_crc64(name.start, name.len), define);
}

Define *get_define(Define_Map *defines, String name)
{
    return defines_get(defines, gb_crc64(name.start, name.len));
}

#define _STRING(x) #x
#define STRING(x) _STRING(x)

void init_std_defines(Define_Map **defines)
{
    u64 time_now = gb_utc_time_now();
    String global = make_string("GLOBAL");
    char *date = date_string(time_now);
    char *time = time_string(time_now);
    add_define(defines, make_string("__DATE__"), make_token_run(date, Token_String), 0, 0, global);
    add_define(defines, make_string("__TIME__"), make_token_run(time, Token_String), 0, 0, global);
    gb_free(gb_heap_allocator(), date);
    gb_free(gb_heap_allocator(), time);

    add_define(defines, make_string("__STDC__"), make_token_run("1", Token_Integer), 0, 0, global);
    add_define(defines, make_string("__STDC_HOSTED__"), make_token_run("1", Token_Integer), 0, 0, global);
    add_define(defines, make_string("__STDC_VERSION__"), make_token_run(STRING(__STDC_VERSION__), Token_Integer), 0, 0, global);

    add_define(defines, make_string("__GNUC__"), make_token_run(STRING(__GNUC__), Token_Integer), 0, 0, global);
    add_define(defines, make_string("__GNUC_MINOR__"), make_token_run(STRING(__GNUC_MINOR__), Token_Integer), 0, 0, global);
    add_define(defines, make_string("__GNUC_PATCHLEVEL__"), make_token_run(STRING(GNUC_PATCHLEVEL__), Token_Integer), 0, 0, global);

#if defined(__unix__)
    add_define(defines, make_string("__unix__"), make_token_run("1", Token_Integer), 0, 0, global);
# if defined(__linux__)
    add_define(defines, make_string("__linux__"), make_token_run("1", Token_Integer), 0, 0, global);
# elif defined(__FreeBSD__)
    add_define(defines, make_string("__FreeBSD__"), make_token_run("1", Token_Integer), 0, 0, global);
# elif defined(__FreeBSD_Kernel__)
    add_define(defines, make_string("__FreeBSD_Kernel__"), make_token_run("1", Token_Integer), 0, 0, global);
# endif
#endif

#if defined(__APPLE__)
    add_define(defines, make_string("__APPLE__"), make_token_run("1", Token_Integer), 0, 0, global);
#endif
#if defined(__MACH__)
    add_define(defines, make_string("__MACH__"), make_token_run("1", Token_Integer), 0, 0, global);
#endif

#if defined(_WIN32)
    add_define(defines, make_string("_WIN32"), make_token_run("1", Token_Integer), 0, 0, global);
#endif
#if defined(_WIN64)
    add_define(defines, make_string("_WIN64"), make_token_run("1", Token_Integer), 0, 0, global);
#endif
#if defined(_MSC_VER)
    add_define(defines, make_string("_MSC_VER"), make_token_run(STRING(_MSC_VER), Token_Integer), 0, 0, global);
#endif
    /*
#if defined(_MSC_EXTENSIONS)
    add_define(defines, make_string("_MSC_EXTENSIONS"), make_token_run("1", Token_Integer), 0, 0, global);
#endif
    */
#if defined(_MSVC_LANG)
    add_define(defines, make_string("_MSVC_LANG"), make_token_run(STRING(_MSVC_LANG), Token_Integer), 0, 0, global);
#endif

    /*
     *  AMD64
     */
#if defined(__x86_64__)
    add_define(defines, make_string("__x86_64__"), make_token_run("1", Token_Integer), 0, 0, global);
#endif
#if defined(__x86_64)
    add_define(defines, make_string("__x86_64"), make_token_run("1", Token_Integer), 0, 0, global);
#endif
#if defined(_AMD64_)
    add_define(defines, make_string("_AMD64_"), make_token_run("1", Token_Integer), 0, 0, global);
#endif
#if defined(__amd64__)
    add_define(defines, make_string("__amd64__"), make_token_run("1", Token_Integer), 0, 0, global);
#endif
#if defined(amd64)
    add_define(defines, make_string("amd64"), make_token_run("1", Token_Integer), 0, 0, global);
#endif
#if defined(__amd64)
    add_define(defines, make_string("__amd64"), make_token_run("1", Token_Integer), 0, 0, global);
#endif
#if defined(_M_X64)
    add_define(defines, make_string("_M_X64"), make_token_run("1", Token_Integer), 0, 0, global);
#endif
#if defined(_M_AMD64)
    add_define(defines, make_string("_M_AMD64"), make_token_run("1", Token_Integer), 0, 0, global);
#endif

    /*
     *  ARM
     */
#if defined(__arm__)
    add_define(defines, make_string("__arm__"), make_token_run("1", Token_Integer), 0, 0, global);
#endif
#if defined(__arm)
    add_define(defines, make_string("__arm"), make_token_run("1", Token_Integer), 0, 0, global);
#endif
#if defined(__thumb__)
    add_define(defines, make_string("__thumb__"), make_token_run("1", Token_Integer), 0, 0, global);
#endif
#if defined(__TARGET_ARCH_ARM)
    add_define(defines, make_string("__TARGET_ARCH_ARM"), make_token_run(STRING(__TARGET_ARCH_ARM), Token_Integer), 0, 0, global);
#endif
#if defined(__TARGET_ARCH_THUMB)
    add_define(defines, make_string("__TARGET_ARCH_THUMB"), make_token_run(STRING(__TARGET_ARCH_THUMB), Token_Integer), 0, 0, global);
#endif
#if defined(_ARM)
    add_define(defines, make_string("_ARM"), make_token_run("1", Token_Integer), 0, 0, global);
#endif
#if defined(_M_ARM)
    add_define(defines, make_string("_M_ARM"), make_token_run(STRING(_M_ARM), Token_Integer), 0, 0, global);
#endif
#if defined(_M_ARMT)
    add_define(defines, make_string("_M_ARMT"), make_token_run("1", Token_Integer), 0, 0, global);
#endif
#if defined(_ARM_)
    add_define(defines, make_string("_ARM_"), make_token_run("1", Token_Integer), 0, 0, global);
#endif
    // ARM64
#if defined(_ARM64_)
    add_define(defines, make_string("_ARM64_"), make_token_run("1", Token_Integer), 0, 0, global);
#endif
#if defined(__aarch64__)
    add_define(defines, make_string("__aarch64__"), make_token_run("1", Token_Integer), 0, 0, global);
#endif
#if defined(_M_ARM64)
    add_define(defines, make_string("_X86_"), make_token_run("1", Token_Integer), 0, 0, global);
#endif

    /*
     *  Intel x86
     */
#if defined(i386)
    add_define(defines, make_string("i386"), make_token_run("1", Token_Integer), 0, 0, global);
#endif
#if defined(__i386)
    add_define(defines, make_string("__i386"), make_token_run("1", Token_Integer), 0, 0, global);
#endif
#if defined(__i386__)
    add_define(defines, make_string("__i386__"), make_token_run("1", Token_Integer), 0, 0, global);
#endif
#if defined(__i486__)
    add_define(defines, make_string("__i486__"), make_token_run("1", Token_Integer), 0, 0, global);
#endif
#if defined(__i586__)
    add_define(defines, make_string("__i586__"), make_token_run("1", Token_Integer), 0, 0, global);
#endif
#if defined(__i686__)
    add_define(defines, make_string("__i686__"), make_token_run("1", Token_Integer), 0, 0, global);
#endif
#if defined(__IA32__)
    add_define(defines, make_string("__IA32__"), make_token_run("1", Token_Integer), 0, 0, global);
#endif
#if defined(_M_IX86)
    add_define(defines, make_string("_M_IX86"), make_token_run(STRING(_M_IX86), Token_Integer), 0, 0, global);
#endif
#if defined(__X86__)
    add_define(defines, make_string("__X86__"), make_token_run("1", Token_Integer), 0, 0, global);
#endif
#if defined(_X86_)
    add_define(defines, make_string("_X86_"), make_token_run("1", Token_Integer), 0, 0, global);
#endif
#if defined(__THW_INTEL__)
    add_define(defines, make_string("__THW_INTEL__"), make_token_run("1", Token_Integer), 0, 0, global);
#endif
#if defined(__I86__)
    add_define(defines, make_string("__I86__"), make_token_run(STRING(__I86__), Token_Integer), 0, 0, global);
#endif
#if defined(__INTEL__)
    add_define(defines, make_string("__INTEL__"), make_token_run("1", Token_Integer), 0, 0, global);
#endif
#if defined(__386)
    add_define(defines, make_string("__386"), make_token_run("1", Token_Integer), 0, 0, global);
#endif


    /*
     *  Intel Itanium (IA-64)
     */
#if defined(__ia64__)
    add_define(defines, make_string("__ia64__"), make_token_run("1", Token_Integer), 0, 0, global);
#endif
#if defined(_IA64)
    add_define(defines, make_string("_IA64"), make_token_run("1", Token_Integer), 0, 0, global);
#endif
#if defined(__IA64__)
    add_define(defines, make_string("__IA64__"), make_token_run("1", Token_Integer), 0, 0, global);
#endif
#if defined(_IA64_)
    add_define(defines, make_string("_IA64_"), make_token_run("1", Token_Integer), 0, 0, global);
#endif
#if defined(__ia64)
    add_define(defines, make_string("__ia64"), make_token_run("1", Token_Integer), 0, 0, global);
#endif
#if defined(_M_IA64)
    add_define(defines, make_string("_M_IA64"), make_token_run(STRING(_M_IA64), Token_Integer), 0, 0, global);
#endif
#if defined(__itanium__)
    add_define(defines, make_string("__itanium__"), make_token_run("1", Token_Integer), 0, 0, global);
#endif

    /*
     *  MISC
     */

#if defined(__64BIT__)
    add_define(defines, make_string("__64BIT__"), make_token_run("1", Token_Integer), 0, 0, global);
#endif
#if defined(__powerpc__)
    add_define(defines, make_string("__powerpc__"), make_token_run("1", Token_Integer), 0, 0, global);
#endif
#if defined(_M_PPC)
    add_define(defines, make_string("_M_PPC"), make_token_run("1", Token_Integer), 0, 0, global);
#endif
#if defined(__powerpc64__)
    add_define(defines, make_string("__powerpc64__"), make_token_run("1", Token_Integer), 0, 0, global);
#endif
#if defined(__ppc64__)
    add_define(defines, make_string("__ppc64__"), make_token_run("1", Token_Integer), 0, 0, global);
#endif

#if defined(__MIPSEL__)
    add_define(defines, make_string("__MIPSEL__"), make_token_run("1", Token_Integer), 0, 0, global);
#endif
#if defined(__mips_isa_rev)
    add_define(defines, make_string("__mips_isa_rev"), make_token_run("1", Token_Integer), 0, 0, global);
#endif
#if defined(__USER_LABEL_PREFIX__)
    add_define(defines, make_string("__USER_LABEL_PREFIX__"), make_token_run(STRING(__USER_LABEL_PREFIX__), Token_String), 0, 0, global);
#endif
}
#undef STRING

gbArray(Define) get_define_list(Define_Map *defines, String whitelist_dir, b32 shallow, gbAllocator alloc)
{
    gbArray(Define) list;
    gb_array_init(list, alloc);
    for (int i = 0; i < gb_array_count(defines->entries); i++)
        if (defines->entries[i].value.in_use &&                                   // Is in use
            !defines->entries[i].value.params &&                                  // Is NOT function style
            cstring_cmp(defines->entries[i].value.file, "GLOBAL") != 0 &&         // Is NOT a globally defined macro
            !is_valid_ident(token_run_string(defines->entries[i].value.value)) && // Is NOT a valid ident
            (!shallow || has_substring(defines->entries[i].value.file, whitelist_dir)) &&       // Is defined in a whitelisted directory
            defines->entries[i].value.value.start)                                // Is NOT zero-length
    {
        gb_array_append(list, defines->entries[i].value);
    }
    return list;
}
