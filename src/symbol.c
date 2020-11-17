#include "symbol.h"
#include "util.h"

gb_inline void add_symbol(Lib *lib, String string)
{
    hashmap_put(lib->symbols, alloc_string(string), &lib->name);
}

b32 read_archive_header(gbFile *file, Coff_Archive_Header *header)
{
    if (!gb_file_read(file, header, 60))
    {
        gb_printf_err("ERROR: Couldn't read COFF archive member header\n");
        return false;
    }

    if (gb_strncmp(header->END, "`\n", 2) != 0)
    {
        gb_printf_err("ERROR: Invalid COFF archive member header\n");
        return false;
    }

    return true;
}

void get_coff_symbols_import_short(gbFile *file, Lib *lib)
{
    Coff_Import_Header header;
    gb_file_read(file, &header, 20);

    char *strings = gb_alloc(gb_heap_allocator(), header.length);
    gb_file_read(file, strings, header.length);

    switch (header.name_type)
    {
    case 1:
        add_symbol(lib, make_string(strings));
        break;
    case 2:
        while (*strings == '?' || *strings == '@') strings++;
        add_symbol(lib, make_string(strings));
        break;
    case 3:
        while (*strings == '?' || *strings == '@') strings++;
        char *end = strings;
        while (*end && *end != '@') end++;
        add_symbol(lib, make_stringn(strings, end-strings));
        break;
    default:
        // gb_printf("NAME_TYPE %d\n", header.name_type);
        // GB_PANIC("PANIC");
        break;
    }
    gb_free(gb_heap_allocator(), strings);
}

void get_coff_symbols_import_long(gbFile *file, Lib *lib, u32 offset)
{
    Coff_Header header;
    gb_file_read(file, &header, 20);

    u32 string_table_offset = header.symtbl_offset + (header.num_symbols * sizeof(Coff_Symbol));
    u32 string_table_length;
    gb_file_read_at(file, &string_table_length, 4, offset+string_table_offset);
    char *string_table = gb_alloc(gb_heap_allocator(), string_table_length);
    gb_file_read(file, string_table, string_table_length);

    gb_file_seek(file, offset + header.symtbl_offset);
    for (u32 i = 0; i < header.num_symbols; i++)
    {
        Coff_Symbol symbol;
        gb_file_read(file, &symbol, sizeof(symbol));
        if (symbol.name.zeroes != 0) // short name
            add_symbol(lib, make_stringn(symbol.name.str, 8));
        else
            add_symbol(lib, make_string(string_table + symbol.name.offset-4));
    }
    gb_free(gb_heap_allocator(), string_table);
}

void get_coff_symbols_import(gbFile *file, Lib *lib, u32 offset)
{
    gb_file_seek(file, offset);

    Coff_Archive_Header header;
    read_archive_header(file, &header);

    char sig[4];
    gb_file_read(file, sig, 4);
    gb_file_skip(file, -4);
    if (gb_strncmp(sig, "\x00\x00\xff\xff", 4) == 0)
        get_coff_symbols_import_short(file, lib);
    else
        get_coff_symbols_import_long(file, lib, offset+60);
}

Lib init_lib(char *filepath)
{
    Lib lib = {0};
    lib.path = make_string_alloc(gb_heap_allocator(), filepath);
    lib.file = str_path_file_name(lib.path);
    lib.name = str_path_base_name(lib.path);
    lib.symbols = hashmap_new(gb_heap_allocator());
    return lib;
}

void get_coff_symbols_lib(gbFile *file, Lib *lib)
{
    char sig[8];
    gb_file_read(file, sig, 8);
    if (gb_strncmp(sig, "!<arch>\n", 8) != 0)
    {
        gb_printf_err("ERROR: Invalid COFF archive signature\n");
        return;
    }

    /* First Linker Member */
    {
        Coff_Archive_Header header;
        if (!read_archive_header(file, &header)) return;
        String size = {0};
        size.start = header.size;
        for (; size.len < sizeof(header.size); size.len++)
            if (header.size[size.len] == ' ') break;

        u32 sz = str_to_int(size);
        sz = sz + sz%2;
        gb_file_skip(file, sz);
    }

    /* Second Linker Member */
    u32 num_members;
    u32 *offsets;
    {
        Coff_Archive_Header header;
        if (!read_archive_header(file, &header)) return;
        gb_file_read(file, &num_members, 4);
        offsets = malloc(num_members * sizeof(u32));
        gb_file_read(file, offsets, num_members * sizeof(u32));
    }

    for (u32 i = 0; i < num_members; i++)
        get_coff_symbols_import(file, lib, offsets[i]);
}

u32 virt_to_phys(u32 virt, Coff_Section *sections, u32 num_sections)
{
    for (u32 i = 0; i < num_sections; i++)
        if (virt >= sections[i].virt_address && virt < sections[i].virt_address + sections[i].virt_size)
            return sections[i].offset + (virt - sections[i].virt_address);
    return 0;
}

void get_coff_symbols_dll(gbFile *file, Lib *lib)
{
    // DOS Signature already read

    // Skip DOS Stub
    {
        u32 offset;
        gb_file_read_at(file, &offset, 4, 0x3c);
        gb_file_seek(file, offset);
    }

    char sig[4];
    gb_file_read(file, sig, 4);
    if (gb_strncmp(sig, "PE\x00\x00", 4) != 0)
    {
        gb_printf_err("ERROR: Invalid DLL signature\n");
        return;
    }

    Coff_Header header;
    Coff_Opt opt_header;
    Coff_Opt_Win win_opt_header;
    Coff_Data_Directories directories;
    gb_file_read(file, &header, sizeof(header));
    gb_file_read(file, &opt_header, sizeof(opt_header));
    gb_file_read(file, &win_opt_header, sizeof(win_opt_header));
    gb_file_read(file, &directories, win_opt_header.num_data_dirs * sizeof(Image_Data_Directory));

    GB_ASSERT(header.machine == 0x8664);  // I assume this won;t work for i386
    GB_ASSERT(opt_header.magic == 0x20b); // Only suport PE32+ currently

    Coff_Section *sections = gb_alloc(gb_heap_allocator(), header.num_sections * sizeof(*sections));
    for (int i = 0; i < header.num_sections; i++)
        gb_file_read(file, &sections[i], sizeof(*sections));
    u32 export_phys = virt_to_phys(directories.export.virt_address, sections, header.num_sections);

    Coff_Export export_table;
    gb_file_read_at(file, &export_table, sizeof(export_table), export_phys);

    u32 name_ptr_phys = virt_to_phys(export_table.name_ptr_rva, sections, header.num_sections);
    u32 *name_ptrs = gb_alloc(gb_heap_allocator(), export_table.num_name_ptrs * 4);
    gb_file_read_at(file, name_ptrs, export_table.num_name_ptrs * 4, name_ptr_phys);

#define MAX_NAME_LENGTH 1024
    char name_temp[MAX_NAME_LENGTH];
    for (u32 i = 0; i < export_table.num_name_ptrs; i++)
    {
        u32 phys = virt_to_phys(name_ptrs[i], sections, header.num_sections);
        gb_file_read_at(file, name_temp, MAX_NAME_LENGTH, phys);
        add_symbol(lib, make_string(name_temp));
    }
}

Lib get_coff_symbols(char *filepath)
{
    Lib lib = {0};
    gbFile file;
    if (gb_file_open(&file, filepath) != gbFileError_None)
    {
        gb_printf_err("ERROR: Could not open library \"%s\"\n", filepath);
        return lib;
    }

    lib = init_lib(filepath);
    char dos_sig[2];
    gb_file_read(&file, dos_sig, 2);
    if (gb_strncmp(dos_sig, "MZ", 2) == 0)
    {
        get_coff_symbols_dll(&file, &lib);
    }
    else
    {
        gb_file_skip(&file, -2);
        get_coff_symbols_lib(&file, &lib);
    }

    return lib;
}
