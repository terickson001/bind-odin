#ifndef _C_BIND_SYMBOL_H_
#define _C_BIND_SYMBOL_H_

#include "gb/gb.h"
#include "strings.h"
#include "hashmap.h"

typedef struct Coff_Header
{
    u16 machine;
    u16 num_sections;
    u32 time;
    u32 symtbl_offset;
    u32 num_symbols;
    u16 opthdr_size;
    u16 flags;
} Coff_Header;

typedef struct Coff_Opt
{
    u16 magic;
    u8 major;
    u8 minor;
    u32 text_length;
    u32 data_length;
    u32 bss_length;
    u32 entry_point;
    u32 text_start;
} Coff_Opt;

typedef struct Coff_Opt_Win
{
    u64 image_base;
    u32 section_align;
    u32 file_align;
    u16 os_major;
    u16 os_minor;
    u16 image_major;
    u16 image_minor;
    u16 subsys_major;
    u16 subsys_minor;
    u32 RESERVED1;
    u32 image_length;
    u32 headers_length;
    u32 checksum;
    u16 subsystem;
    u16 dll_flags;
    u64 stack_reserve;
    u64 stack_commit;
    u64 heap_reserve;
    u64 heap_commit;
    u32 RESERVED2;
    u32 num_data_dirs;
} Coff_Opt_Win;

enum Coff_Win_DLL_Flags
{
    DllFlag_High_Entropy_VA = 0x0020,
    DllFlag_Dynamic_Base = 0x0040,
    DllFlag_Force_Integrity = 0x0080,
    DllFlag_NX_Compat = 0x0100,
    DllFlag_No_Isolation = 0x0200,
    DllFlag_No_SEH = 0x0400,
    DllFlag_No_Bind = 0x0800,
    DllFlag_App_Container = 0x1000,
    DllFlag_WDM_Driver = 0x2000,
    DllFlag_Guard_CF = 0x4000,
    DllFlag_Terminal_Server_Aware = 0x8000
};

typedef struct Image_Data_Directory
{
    u32 virt_address;
    u32 length;
} Image_Data_Directory;

typedef struct Coff_Data_Directories
{
    Image_Data_Directory export;
    Image_Data_Directory import;
    Image_Data_Directory resource;
    Image_Data_Directory exception;
    u64 certificate;
    Image_Data_Directory relocation;
    Image_Data_Directory debug;
    Image_Data_Directory arch;
    Image_Data_Directory global_ptr;
    Image_Data_Directory tls;
    Image_Data_Directory load_config;
    Image_Data_Directory bound_import;
    Image_Data_Directory iat;
    Image_Data_Directory delay_import;
    Image_Data_Directory clr_runtime;
    Image_Data_Directory RESERVED;
} Coff_Data_Directories;

typedef struct Coff_Export
{
    u32 RESERVED;
    u32 time;
    u16 major;
    u16 minor;
    u32 name_rva;
    u32 ordinal_base;
    u32 num_addresses;
    u32 num_name_ptrs;
    u32 address_rva;
    u32 name_ptr_rva;
    u32 ordinal_rva;
} Coff_Export;
typedef union Coff_Sym_Name
{
    char str[8];
    struct
    {
        u32 zeroes;
        u32 offset;
    };
} Coff_Sym_Name;

typedef struct Coff_Section
{
    Coff_Sym_Name name;
    u32 virt_size;
    u32 virt_address;
    u32 length;
    u32 offset;
    u32 relocation_offset;
    u32 linum_offset;
    u16 num_relocation_entries;
    u16 num_linum_entries;
    u32 flags;
} Coff_Section;

enum Coff_Section_Flags
{
    SectionType_Text = 0x20,
    SectionType_Data = 0x40,
    SectionType_BSS  = 0x80
};

typedef struct Coff_Relocation
{
    u32 virt_addr;
    u32 symbol_idx;
    u16 type;
} Coff_Relocation;

typedef struct Coff_Linum
{
    union
    {
        i32 symbol_index;
        i32 phys_addr;
    } addr;
    u16 linum;
} Coff_Linum;

#if defined(GB_COMPILER_MSVC)
#define PACK(STRUCT) __pragma(pack(push,1)) STRUCT __pragma(pack(pop))
#else
#define PACK(STRUCT) STRUCT __attribute__((__packed__))
#endif
PACK(
typedef struct Coff_Symbol
{
    Coff_Sym_Name name;
    i32 value;
    i16 section_num;
    u16 type;
    u8 class;
    u8 auxiliary_count;
} Coff_Symbol;
)

enum Coff_Section_Number
{
    SectionNumber_Debug = -2,
    SectionNumber_Absolute = -1,
    SectionNumber_Undefined = 0
};

enum Coff_Storage_Class
{
    StorageClass_Function_End = -1,
    StorageClass_Null = 0,
    StorageClass_Automatic,
    StorageClass_External,
    StorageClass_Static,
    StorageClass_Register,
    StorageClass_External_Def,
    StorageClass_Label,
    StorageClass_Undefined_Label,
    StorageClass_Struct_Member,
    StorageClass_Argument,
    StorageClass_Struct_Tag,
    StorageClass_Union_Member,
    StorageClass_Union_Tag,
    StorageClass_Typedef,
    StorageClass_Undefined_Static,
    StorageClass_Enum_Tag,
    StorageClass_Enum_Member,
    StorageClass_Register_Parameter,
    StorageClass_Bit_Field,
    StorageClass_Block = 100,
    StorageClass_Function,
    StorageClass_Struct_End,
    StorageClass_File,
    StorageClass_Section,
    StorageClass_Weak_External,
    StorageClass_CLR_Token = 107
};

enum Coff_Symbol_Type
{
    SymbolType_Null = 0x0,
    SymbolType_Void = 0x1,
    SymbolType_Char = 0x2,
    SymbolType_Short = 0x3,
    SymbolType_Int = 0x4,
    SymbolType_Long = 0x5,
    SymbolType_Float = 0x6,
    SymbolType_Double = 0x7,
    SymbolType_Struct = 0x8,
    SymbolType_Union = 0x9,
    SymbolType_Enum = 0xA,
    SymbolType_MOE = 0xB,
    SymbolType_Byte = 0xC,
    SymbolType_Word = 0xD,
    SymbolType_Uint = 0xE,
    SymbolType_Dword = 0xF,

    SymbolType_Pointer = 0x10,
    SymbolType_Function = 0x20,
    SymbolType_Array = 0x30
};

typedef struct Coff_Aux_Function
{
    u32 begin_idx;
    u32 text_length;
    u32 linum_offset;
    u32 next_func_offset;
    u16 __padding__;
} Coff_Aux_Function;

typedef struct Coff_Import_Directory
{
    u32 lookup_table;
    u32 time;
    u32 forwarder_idx;
    u32 name_offset;
    u32 address_table;
} Coff_Import_Directory;

typedef union Coff_Import_Lookup_32
{
    u32 flags;
    struct
    {
        int ordinal : 1;
        union
        {
            int ordinal_number : 31;
            int name_table_address : 31;
        };
    };
} Coff_Import_Lookup_32;

typedef struct Coff_Name_Entry
{
    u16 hint;
    char name[0];
} Coff_Name_Entry;


typedef struct Coff_Import_Header
{
    u16 sig1;
    u16 sig2;
    u16 version;
    u16 arch;
    u32 time;
    u32 length;
    u16 hint;
    union
    {
        u16 flags;
        struct
        {
            u16 type : 2;
            u16 name_type : 3;
            u16 reserved : 11;
        };
    };
} Coff_Import_Header;

typedef struct Coff_Archive_Header
{
    char name[16];
    char data[12];
    char user_id[6];
    char group_id[6];
    char mode[8];
    char size[10];
    char END[2];
} Coff_Archive_Header;

typedef struct Lib
{
    String path;
    String file;
    String name;
    map_t symbols;
} Lib;


Lib get_coff_symbols(char *filepath);

#endif
