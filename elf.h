#include "arkad.h"
#include "icore.h"
#include <string>


#ifndef __ELFH__
#define __ELFH__

#define DW_ATE_boolean       			0x02
#define DW_ATE_signed        			0x05
#define DW_ATE_unsigned      			0x07
#define DW_ATE_unsigned_char 			0x08
#define DW_TAG_array_type             	0x01
#define DW_TAG_enumeration_type       	0x04
#define DW_TAG_formal_parameter       	0x05
#define DW_TAG_label                  	0x0a
#define DW_TAG_lexical_block          	0x0b
#define DW_TAG_member                 	0x0d
#define DW_TAG_pointer_type           	0x0f
#define DW_TAG_reference_type         	0x10
#define DW_TAG_compile_unit           	0x11
#define DW_TAG_structure_type         	0x13
#define DW_TAG_subroutine_type        	0x15
#define DW_TAG_typedef                	0x16
#define DW_TAG_union_type             	0x17
#define DW_TAG_unspecified_parameters 	0x18
#define DW_TAG_inheritance            	0x1c
#define DW_TAG_inlined_subroutine     	0x1d
#define DW_TAG_subrange_type          	0x21
#define DW_TAG_base_type              	0x24
#define DW_TAG_const_type             	0x26
#define DW_TAG_enumerator             	0x28
#define DW_TAG_subprogram             	0x2e
#define DW_TAG_variable               	0x34
#define DW_TAG_volatile_type          	0x35

#define DW_AT_sibling              	0x01
#define DW_AT_location             	0x02
#define DW_AT_name                 	0x03
#define DW_AT_byte_size            	0x0b
#define DW_AT_bit_offset           	0x0c
#define DW_AT_bit_size             	0x0d
#define DW_AT_stmt_list            	0x10
#define DW_AT_low_pc               	0x11
#define DW_AT_high_pc              	0x12
#define DW_AT_language             	0x13
#define DW_AT_compdir              	0x1b
#define DW_AT_const_value          	0x1c
#define DW_AT_containing_type      	0x1d
#define DW_AT_inline               	0x20
#define DW_AT_producer             	0x25
#define DW_AT_prototyped           	0x27
#define DW_AT_upper_bound          	0x2f
#define DW_AT_abstract_origin      	0x31
#define DW_AT_accessibility        	0x32
#define DW_AT_artificial           	0x34
#define DW_AT_data_member_location 	0x38
#define DW_AT_decl_file            	0x3a
#define DW_AT_decl_line            	0x3b
#define DW_AT_declaration          	0x3c
#define DW_AT_encoding             	0x3e
#define DW_AT_external             	0x3f
#define DW_AT_frame_base           	0x40
#define DW_AT_macro_info           	0x43
#define DW_AT_specification        	0x47
#define DW_AT_type                 	0x49
#define DW_AT_virtuality           	0x4c
#define DW_AT_vtable_elem_location 	0x4d
// DWARF 2.1/3.0 extensions
#define DW_AT_entry_pc             	0x52
#define DW_AT_ranges               	0x55
// ARM Compiler extensions
#define DW_AT_proc_body            	0x2000
#define DW_AT_save_offset          	0x2001
#define DW_AT_user_2002            	0x2002
// MIPS extensions
#define DW_AT_MIPS_linkage_name    	0x2007

#define DW_FORM_addr      				0x01
#define DW_FORM_data2     				0x05
#define DW_FORM_data4     				0x06
#define DW_FORM_string    0x08
#define DW_FORM_block     0x09
#define DW_FORM_block1    0x0a
#define DW_FORM_data1     0x0b
#define DW_FORM_flag      0x0c
#define DW_FORM_sdata     0x0d
#define DW_FORM_strp      0x0e
#define DW_FORM_udata     0x0f
#define DW_FORM_ref_addr  0x10
#define DW_FORM_ref4      0x13
#define DW_FORM_ref_udata 0x15
#define DW_FORM_indirect  0x16

#define DW_OP_addr                      0x03
#define DW_OP_deref                     0x06
#define DW_OP_const1u                   0x08
#define DW_OP_const1s                   0x09
#define DW_OP_const2u                   0x0a
#define DW_OP_const2s                   0x0b
#define DW_OP_const4u                   0x0c
#define DW_OP_const4s                   0x0d
#define DW_OP_const8u                   0x0e
#define DW_OP_const8s                   0x0f
#define DW_OP_constu                    0x10
#define DW_OP_consts                    0x11
#define DW_OP_dup                       0x12
#define DW_OP_drop                      0x13
#define DW_OP_over                      0x14
#define DW_OP_pick                      0x15
#define DW_OP_swap                      0x16
#define DW_OP_rot                       0x17
#define DW_OP_xderef                    0x18
#define DW_OP_abs                       0x19
#define DW_OP_and                       0x1a
#define DW_OP_div                       0x1b
#define DW_OP_minus                     0x1c
#define DW_OP_mod                       0x1d
#define DW_OP_mul                       0x1e
#define DW_OP_neg                       0x1f
#define DW_OP_not                       0x20
#define DW_OP_or                        0x21
#define DW_OP_plus                      0x22
#define DW_OP_plus_uconst               0x23
#define DW_OP_shl                       0x24
#define DW_OP_shr                       0x25
#define DW_OP_shra                      0x26
#define DW_OP_xor                       0x27
#define DW_OP_bra                       0x28
#define DW_OP_eq                        0x29
#define DW_OP_ge                        0x2a
#define DW_OP_gt                        0x2b
#define DW_OP_le                        0x2c
#define DW_OP_lt                        0x2d
#define DW_OP_ne                        0x2e
#define DW_OP_skip                      0x2f
#define DW_OP_lit0                      0x30
#define DW_OP_lit1                      0x31
#define DW_OP_lit2                      0x32
#define DW_OP_lit3                      0x33
#define DW_OP_lit4                      0x34
#define DW_OP_lit5                      0x35
#define DW_OP_lit6                      0x36
#define DW_OP_lit7                      0x37
#define DW_OP_lit8                      0x38
#define DW_OP_lit9                      0x39
#define DW_OP_lit10                     0x3a
#define DW_OP_lit11                     0x3b
#define DW_OP_lit12                     0x3c
#define DW_OP_lit13                     0x3d
#define DW_OP_lit14                     0x3e
#define DW_OP_lit15                     0x3f
#define DW_OP_lit16                     0x40
#define DW_OP_lit17                     0x41
#define DW_OP_lit18                     0x42
#define DW_OP_lit19                     0x43
#define DW_OP_lit20                     0x44
#define DW_OP_lit21                     0x45
#define DW_OP_lit22                     0x46
#define DW_OP_lit23                     0x47
#define DW_OP_lit24                     0x48
#define DW_OP_lit25                     0x49
#define DW_OP_lit26                     0x4a
#define DW_OP_lit27                     0x4b
#define DW_OP_lit28                     0x4c
#define DW_OP_lit29                     0x4d
#define DW_OP_lit30                     0x4e
#define DW_OP_lit31                     0x4f
#define DW_OP_reg0                      0x50
#define DW_OP_reg1                      0x51
#define DW_OP_reg2                      0x52
#define DW_OP_reg3                      0x53
#define DW_OP_reg4                      0x54
#define DW_OP_reg5                      0x55
#define DW_OP_reg6                      0x56
#define DW_OP_reg7                      0x57
#define DW_OP_reg8                      0x58
#define DW_OP_reg9                      0x59
#define DW_OP_reg10                     0x5a
#define DW_OP_reg11                     0x5b
#define DW_OP_reg12                     0x5c
#define DW_OP_reg13                     0x5d
#define DW_OP_reg14                     0x5e
#define DW_OP_reg15                     0x5f
#define DW_OP_reg16                     0x60
#define DW_OP_reg17                     0x61
#define DW_OP_reg18                     0x62
#define DW_OP_reg19                     0x63
#define DW_OP_reg20                     0x64
#define DW_OP_reg21                     0x65
#define DW_OP_reg22                     0x66
#define DW_OP_reg23                     0x67
#define DW_OP_reg24                     0x68
#define DW_OP_reg25                     0x69
#define DW_OP_reg26                     0x6a
#define DW_OP_reg27                     0x6b
#define DW_OP_reg28                     0x6c
#define DW_OP_reg29                     0x6d
#define DW_OP_reg30                     0x6e
#define DW_OP_reg31                     0x6f
#define DW_OP_breg0                     0x70
#define DW_OP_breg1                     0x71
#define DW_OP_breg2                     0x72
#define DW_OP_breg3                     0x73
#define DW_OP_breg4                     0x74
#define DW_OP_breg5                     0x75
#define DW_OP_breg6                     0x76
#define DW_OP_breg7                     0x77
#define DW_OP_breg8                     0x78
#define DW_OP_breg9                     0x79
#define DW_OP_breg10                    0x7a
#define DW_OP_breg11                    0x7b
#define DW_OP_breg12                    0x7c
#define DW_OP_breg13                    0x7d
#define DW_OP_breg14                    0x7e
#define DW_OP_breg15                    0x7f
#define DW_OP_breg16                    0x80
#define DW_OP_breg17                    0x81
#define DW_OP_breg18                    0x82
#define DW_OP_breg19                    0x83
#define DW_OP_breg20                    0x84
#define DW_OP_breg21                    0x85
#define DW_OP_breg22                    0x86
#define DW_OP_breg23                    0x87
#define DW_OP_breg24                    0x88
#define DW_OP_breg25                    0x89
#define DW_OP_breg26                    0x8a
#define DW_OP_breg27                    0x8b
#define DW_OP_breg28                    0x8c
#define DW_OP_breg29                    0x8d
#define DW_OP_breg30                    0x8e
#define DW_OP_breg31                    0x8f
#define DW_OP_regx                      0x90
#define DW_OP_fbreg                     0x91
#define DW_OP_bregx                     0x92
#define DW_OP_piece                     0x93
#define DW_OP_deref_size                0x94
#define DW_OP_xderef_size               0x95
#define DW_OP_nop                       0x96
#define DW_OP_push_object_address       0x97 /* DWARF3 */
#define DW_OP_call2                     0x98 /* DWARF3 */
#define DW_OP_call4                     0x99 /* DWARF3 */
#define DW_OP_call_ref                  0x9a /* DWARF3 */
#define DW_OP_form_tls_address          0x9b /* DWARF3f */
#define DW_OP_call_frame_cfa            0x9c /* DWARF3f */
#define DW_OP_bit_piece                 0x9d /* DWARF3f */

#define DW_LNS_extended_op      	0x00
#define DW_LNS_copy             	0x01
#define DW_LNS_advance_pc       	0x02
#define DW_LNS_advance_line     	0x03
#define DW_LNS_set_file         	0x04
#define DW_LNS_set_column       	0x05
#define DW_LNS_negate_stmt      	0x06
#define DW_LNS_set_basic_block  	0x07
#define DW_LNS_const_add_pc     	0x08
#define DW_LNS_fixed_advance_pc 	0x09
#define DW_LNS_set_prologue_end 	0x0A
#define DW_LNS_set_epilogue_begin 	0x0B
#define DW_LNS_set_isa 			0x0C

#define DW_LNE_end_sequence 0x01
#define DW_LNE_set_address  0x02
#define DW_LNE_define_file  0x03

#define DW_CFA_advance_loc      0x01
#define DW_CFA_offset           0x02
#define DW_CFA_restore          0x03
#define DW_CFA_set_loc          0x01
#define DW_CFA_advance_loc1     0x02
#define DW_CFA_advance_loc2     0x03
#define DW_CFA_advance_loc4     0x04
#define DW_CFA_offset_extended  0x05
#define DW_CFA_restore_extended 0x06
#define DW_CFA_undefined        0x07
#define DW_CFA_same_value       0x08
#define DW_CFA_register         0x09
#define DW_CFA_remember_state   0x0a
#define DW_CFA_restore_state    0x0b
#define DW_CFA_def_cfa          0x0c
#define DW_CFA_def_cfa_register 0x0d
#define DW_CFA_def_cfa_offset   0x0e
#define DW_CFA_nop              0x00

#define CASE_TYPE_TAG \
    case DW_TAG_const_type:\
    case DW_TAG_volatile_type:\
    case DW_TAG_pointer_type:\
    case DW_TAG_base_type:\
    case DW_TAG_array_type:\
    case DW_TAG_structure_type:\
    case DW_TAG_union_type:\
    case DW_TAG_typedef:\
    case DW_TAG_subroutine_type:\
    case DW_TAG_enumeration_type:\
    case DW_TAG_enumerator:\
    case DW_TAG_reference_type

#define DW_ATE_address                  0x1
#define DW_ATE_complex_float            0x3
#define DW_ATE_float                    0x4
#define DW_ATE_signed_char              0x6
#define DW_ATE_imaginary_float          0x9  /* DWARF3 */
#define DW_ATE_packed_decimal           0xa  /* DWARF3f */
#define DW_ATE_numeric_string           0xb  /* DWARF3f */
#define DW_ATE_edited                   0xc  /* DWARF3f */
#define DW_ATE_signed_fixed             0xd  /* DWARF3f */
#define DW_ATE_unsigned_fixed           0xe  /* DWARF3f */
#define DW_ATE_decimal_float            0xf  /* DWARF3f */

using namespace std;

enum LocationType {
  LOCATION_register,
  LOCATION_memory,
  LOCATION_value
};

struct ELFcie {
  ELFcie *next;
  u32 offset;
  u8 *augmentation;
  u32 codeAlign;
  s32 dataAlign;
  int returnAddress;
  u8 *data;
  u32 dataLen;
};

struct ELFfde {
  ELFcie *cie;
  u32 address;
  u32 end;
  u8 *data;
  u32 dataLen;
};

enum ELFRegMode {
  REG_NOT_SET,
  REG_OFFSET,
  REG_REGISTER
};

struct ELFFrameStateRegister {
  ELFRegMode mode;
  int reg;
  s32 offset;
};

struct ELFFrameStateRegisters {
  ELFFrameStateRegister regs[16];
  ELFFrameStateRegisters *previous;
};

enum ELFCfaMode {
  CFA_NOT_SET,
  CFA_REG_OFFSET
};

struct ELFFrameState {
  ELFFrameStateRegisters registers;
  ELFCfaMode cfaMode;
  int cfaRegister;
  s32 cfaOffset;
  u32 pc;
  int dataAlign;
  int codeAlign;
  int returnAddress;
};

struct ELFHeader {
  u32 magic;
  u8 clazz;
  u8 data;
  u8 version;
  u8 pad[9];
  u16 e_type;
  u16 e_machine;
  u32 e_version;
  u32 e_entry;
  u32 e_phoff;
  u32 e_shoff;
  u32 e_flags;
  u16 e_ehsize;
  u16 e_phentsize;
  u16 e_phnum;
  u16 e_shentsize;
  u16 e_shnum;
  u16 e_shstrndx;
};

struct ELFProgramHeader {
  u32 type;
  u32 offset;
  u32 vaddr;
  u32 paddr;
  u32 filesz;
  u32 memsz;
  u32 flags;
  u32 align;
};

struct ELFSectionHeader {
  u32 name;
  u32 type;
  u32 flags;
  u32 addr;
  u32 offset;
  u32 size;
  u32 link;
  u32 info;
  u32 addralign;
  u32 entsize;
};

struct ELFSymbol {
  u32 name;
  u32 value;
  u32 size;
  u8 info;
  u8 other;
  u16 shndx;
};

struct ELFBlock {
  int length;
  u8 *data;
};

struct ELFLocation {
   u32 offset,lowPC,highPC;
   ELFBlock *block;
};

struct ELFAttr {
  u32 name;
  u32 form;
  union {
    u32 value;
    char *string;
    u8 *data;
    bool flag;
    ELFBlock *block;
  };
};

struct ELFAbbrev {
  u32 number;
  u32 tag;
  bool hasChildren;
  int numAttrs;
  ELFAttr *attrs;
  ELFAbbrev *next;
};

enum TypeEnum {
  TYPE_base,
  TYPE_pointer,
  TYPE_function,
  TYPE_void,
  TYPE_array,
  TYPE_struct,
  TYPE_reference,
  TYPE_enum,
  TYPE_union
};

struct Type;
struct elf_Object;

struct FunctionType {
  Type *returnType;
  elf_Object *args;
};

struct Member {
  char *name;
  Type *type;
  int bitSize;
  int bitOffset;
  int byteSize;
  ELFBlock *location;
};

struct Struct {
  int memberCount;
  Member *members;
};

struct Array {
  Type *type;
  int maxBounds;
  int *bounds;
};

struct EnumMember {
  char *name;
  u32 value;
};

struct Enum {
  int count;
  EnumMember *members;
};

struct Type {
  u32 offset;
  TypeEnum type;
  char *name;
  int encoding;
  int size;
  int bitSize;
  int file;
  int line;
  union {
    Type *pointer;
    FunctionType *function;
    Array *array;
    Struct *structure;
    Enum *enumeration;
  };
  Type *next;
};

struct elf_Object {
  char *name;
  int file;
  int line;
  bool external;
  Type *type;
  ELFBlock *location;
  u32 startScope;
  u32 endScope;
  elf_Object *next;
};

struct Function {
  char *name;
  u32 lowPC;
  u32 highPC;
  int file;
  int line;
  bool external;
  Type *returnType;
  elf_Object *parameters;
  elf_Object *variables;
  ELFBlock *frameBase;
  Function *next;
};

struct LineInfoItem {
  u32 address;
  int file;
  int line;
};

struct LineInfoFileItem {
	char *name;
   int dir;
};

struct LineInfo {
  int fileCount;
  LineInfoFileItem *files;
  int number;
  char **dirs;
  int dirCount;
  LineInfoItem *lines;
};

struct ARange {
  u32 lowPC;
  u32 highPC;
};

struct ARanges {
  u32 offset;
  int count;
  ARange *ranges;
};

struct CompileUnit {
  u32 length;
  u32 top;
  u32 offset;
  ELFAbbrev **abbrevs;
  ARanges *ranges;
  char *name;
  char *compdir;
  u32 lowPC;
  u32 highPC;
  bool hasLineInfo;
  u32 lineInfo;
  LineInfo *lineInfoTable;
  Function *functions;
  Function *lastFunction;
  elf_Object *variables;
  Type *types;
  CompileUnit *next;
};

struct DebugInfo {
  u8 *debugfile;
  u8 *abbrevdata;
  u32 debugdata;
  u32 infodata;
  int numRanges;
  ARanges *ranges;
};

struct Symbol {
  char *name;
  int type;
  int binding;
  u32 address;
  u32 value;
  u32 size;
};

class LElfFile {
public:
	LElfFile(IStreamer *);
	virtual ~LElfFile();
	int Open();
	int Close();
	u32 Load(void *);
	int GetCurrentFunction(u32 addr,void **f,void **u,void **l);
	int GetFunctionFromName(const char *name,void **f,void **u);
	int GetVariableFromName(const char *name,u32 adr,void **obj,void **sym,void **u);
	int _getInfo(u32 *,void **);
protected:
	u32 Read(LPVOID lpBuffer,u32 dwBytes);
	u32 Seek(LONG dwDistanceToMove = 0,u32 dwMoveMethod = FILE_BEGIN);
	CompileUnit *GetCompileUnitForData(u32 data);
	CompileUnit *GetCompileUnit(u32 addr);
	HLOCAL LocalReAlloc(HLOCAL hMem,u32 uBytes,u32 uFlags);
	ELFSectionHeader *GetSectionByName(char *name);
	u32 ReadSection(ELFSectionHeader *h,u32 startOfs = 0){return startOfs + h->offset;};
	CompileUnit *ParseCompUnit(u32 data,u32 abbrevData);
	u32 Read4Bytes(u32 ofs);
	u16 Read2Bytes(u32 ofs);
	ELFAbbrev **ReadAbbrevs(u32 data,u32 offset);
	u32 ReadLEB128(u32 data,int *bytesRead);
	LONG ReadSignedLEB128(u32 data,int *bytesRead);
	ELFAbbrev *GetAbbrev(ELFAbbrev **table,u32 number);
	u32 ReadAttribute(u32 data, ELFAttr *attr);
	u32 SeekRead(u32 ofs,LPVOID lpBuffer,u32 dwBytes){if(Seek(ofs,FILE_BEGIN) == 0xFFFFFFFF) return 0;return Read(lpBuffer,dwBytes);};
	char *ReadString(u32 ofs);
	void ParseLineInfo(CompileUnit *unit,u32 top);
	u32 ParseCompileUnitChildren(u32 data, CompileUnit *unit);
	u32 ParseFunction(u32 data, ELFAbbrev *abbrev, CompileUnit *unit,Function **f);
	u32 SkipData(u32 data, ELFAbbrev *abbrev, ELFAbbrev **abbrevs);
	u32 ParseObject(u32 data, ELFAbbrev *abbrev, CompileUnit *unit,elf_Object **object);
	u32 ParseUnknownData(u32 data, ELFAbbrev *abbrev, ELFAbbrev **abbrevs);
	void CleanUp(elf_Object *o);
	void CleanUp(Function *func);
	u32 ParseBlock(u32 data,ELFAbbrev *abbrev,CompileUnit *unit,Function *func,elf_Object **lastVar);
	Type* ParseType(CompileUnit *unit,u32 offset);
	void GetObjectAttributes(CompileUnit *unit,u32 offset, elf_Object *o);
	void AddType(Type *type, CompileUnit *unit, u32 offset);
	void ParseType(u32 data,u32 offset,ELFAbbrev *abbrev,CompileUnit *unit,Type **type);
	void AddLine(LineInfo *l,u32 a, int file, int line, int *max);
	void GetFunctionAttributes(CompileUnit *unit, u32 offset, Function *func);
	void ParseAranges(u32 data);
	void ParseCFA(u32 top);
	void ReadSymtab(u32 data);
	void ParseLocations(u32 top);
	ELFSectionHeader *GetSectionByNumber(int number);


	ELFHeader header;
	ELFProgramHeader *ProgramHeader;
	ELFSectionHeader *SectionHeader,*dbgHeader;
	DebugInfo dbgInfo;
	u32 dbgStringsOfs;
	CompileUnit *CurrentUnit,*CompileUnits;
	ELFcie *Cies;
	ELFfde **Fdes;
	int FdeCount,SymbolsCount,LocationsCount;
  	Symbol *Symbols;
	ELFLocation *Locations;
	char *SymbolsStrTab,*cache;
	u32 dwCachePos,dwCacheStart,dwCacheEnd;

	IStreamer *_stream;
};

/*extern u32 elfReadLEB128(u8 *, int *);
extern s32 elfReadSignedLEB128(u8 *, int *);
extern bool elfRead(const char *, int &, LStream *pFile);
extern bool elfGetSymbolAddress(char *,u32 *, u32 *, int *);
extern char *elfGetAddressSymbol(u32);
extern char *elfGetSymbol(int, u32 *, u32 *, int *);
extern void elfCleanUp();
extern bool elfGetCurrentFunction(u32, Function **, CompileUnit **c);
extern bool elfGetObject(char *, Function *, CompileUnit *, Object **);
extern bool elfFindLineInUnit(u32 *, CompileUnit *, int);
extern bool elfFindLineInModule(u32 *, char *, int);
u32 elfDecodeLocation(Function *, ELFBlock *, LocationType *);
u32 elfDecodeLocation(Function *, ELFBlock *, LocationType *, u32);
int elfFindLine(CompileUnit *unit, Function *func, u32 addr, char **);*/

#endif
