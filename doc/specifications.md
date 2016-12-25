
# Introduction
The documents outlines the requirements and implementation advice for an open source
software development library and toolchain for creating object code for the PS Vita (c) device.

## Goals
The main goal for this project is to create an ecosystem of amateur produced software and
games (homebrew ) on the PS Vita (c) device for non-commercial purposes. The inspiration
for this document comes from observed failures of open toolchains on other gaming plat-
forms. The goal is to define precisely the requirements and implementation of an open
source toolchain and SDK for the PS Vita (c) . Collaboration from the community is expected
and desired in the creation of this ecosystem. Comments and suggestions for this document
should be sent directly to the author.

## Legal
PS Vita (c) is a trademark of Sony Computer Entertainment America LLC. This document is
written independently of and is not approved by SCEA. Please don’t sue us.

## Overview
The PS Vita (c) carries an ARM Cortex A9 MPCore as the main CPU processor in a custom
SoC. The processor implements the ARMv7-R architecture (with full Thumb2 support).
Additionally, it supports the MPE, NEONv1, and VFPv3 extensions.
The software infrastructure is handed by a proprietary operating system; the details of
which is outside the scope of this document. What this document will define is the executable
format, which is an extension of the ELF version 1 standards. Thorough knowledge of the
ELF specifications[1] is assumed and the SCE extensions will be described in detail. The
simplified specifications[2] and ARM extensions to ELF[3] will be referenced throughout this
document.
The first part of this document will describe the format of SCE ELF executables including
details on SCE extension segment and sections. The second part will detail a proposed SDK
format for writing the include files and symbol-NID mapping database. The third part will
specify a tool which can convert a standard Linux EABI ELF into a SCE ELF.

## SCE ELF Format
ELF Header
The header is a standard ARM ELF[3] header. For the e type field, there are some additional
options.

```
//TODO
```

There are three filename extensions for SELFs.
- self is usually used for application executables and likely stands for “secure ELF” or “Sony ELF”.
- suprx are userland dynamic libraries and is similar to how so libraries work on Linux.
- skprx are kernel modules and is similar to how ko libraries work on Linux.
Even though the extensions are different, all these file types use the same SCE ELF format.
There is no difference between an ET SCE RELEXEC application and a suprx except that suprx
usually exports additional libraries for linking.
That means, in theory, a single SCE ELF can act as both an application and a userland
library. The only difference between suprx and skprx is that skprx is meant to run in kernel
and can use ARM system instructions. A skprx can also export libraries to userland in the
form of syscalls (more information below).

```
TODO
```

The toolchain is required to support SHT SCE RELA, which is how relocations are imple-
mented in SCE ELFs. The details are described in the following subsection.
### SCE Relocations
SCE ELFs use a different relocation format from standard ELFs. The relocation entries
are in two different format, either an 8 byte “short” entry or a 12 byte “long” entry. You
are allowed to mix and match “short” and “long” entries, but that is not recommended for
alignment reasons. The entire relocation segment is just a packed array of these entries.
In the short entry, the offset is stored partially in the first word and partially in the second
word. It also has a 12-bit addend. In the long entry, there is support for two relocations
on the same data. The open toolchain does not have to implement this. Long entries have
32-bit addends.

- `r_format` determines the entry format. Currently two are supported: 0x0 is “long entry” and 0x1 is “short entry”. In previous versions of this document, this field was called r short.
- `r_symseg` is the index of the program segment containing the data to point to. Previous versions of this documented noted that if this value is 0xF, then 0x0 is used as the base address. This is no longer true in recent system versions.
- `r_code` is the relocation code defined in ARM ELF[3]
- `r_datseg` is the index of the program segment containing the pointer that is to be relocated.
- `r_offset` is the offset into the segment indexed by r datseg. This is the pointer to relocate.
- `r_addend` is the offset into the segment indexed by r symseg. This is what is written to the relocated pointer.

### Relocation Operations
Only the following ARM relocation types are supported on the PS Vita (c) :
The toolchain is required to only output relocations of these types. Refer to that ARM
ELF[3] manual for information on how the value is formed. The definitions of the variables
for relocation is as follows.

Segment start = Base address of segment indexed at r datseg
Symbol start = Base address of segment indexed at r symseg

- `P` = segment start + r offset
- `S` = symbol start
- `A` = r addend

## SCE Dynamic Section
ELF Dynamic sections are not used (a change from PSP). Instead all dynamic linking infor-
mation is stored as part of the export and import sections, which are SHT PROGBITS sections.

```
TODO
```

### NIDs
Instead of using symbols, SCE ELF linking depends on NIDs. These are just 32-bit integers
created from hashing the symbol name. The formula for generating them does not matter as
long as they match up. For our purposes, we will make sure our open SDK recognizes NIDs
for imported functions and when NIDs are created, they can be done so in an implementation
defined way.

### Module Information
The first SCE specific section .sceModuleInfo.rodata is located in the same program seg-
ment as .text. It contains metadata on the module 1
Some fields here are optional and can be set to zero. The other fields determine how this
module is loaded and linked. All offset fields are formatted as follows: top 2 bits is an index
to the segment to start at and bottom 30 bits is an offset from the segment. Currently, the
segment start index must match the segment that the module information structure is in.

- `version`: Set to 0x0101
- `name`: Name of the module
- `type`: 0x0 for executable, 0x6 for PRX

Previous versions of this document swapped the usage of the term “library” and “module”. This change is to be more consistent with Sony’s usage of the term.

```
TODO
```

- `export_top`: Offset to start of export table.
- `export_end`: Offset to end of export table.
- `import_top`: Offset to start of import table.
- `import_end`: Offset to start of import table.
- `module_nid`: NID of this module. Can be a random unique integer. This can freely change with version increments. It is not used for imports.
- `module_start`: Offset to function to run when module is started. Set to 0 to disable.
- `module_stop`: Offset to function to run when module is exiting. Set to 0 to disable.
- `exidx_top`: Offset to start of ARM EXIDX (optional)
- `exidx_end`: Offset to end of ARM EXIDX (optional)
- `extab_top`: Offset to start of ARM EXTAB (optional)
- `extab_end`: Offset to end of ARM EXTAB (optional)

Each module can export one or more libraries. To get the start of the export table, we add
export top to the base of the segment address. To iterate through the export tables, we
read the size field of each entry and increment by the size until we reach export end.

```
TODO
```

- `size`: Set to 0x20. There are other sized export tables that follow different formats. We will not support them for now.
- `version`: Set to 0x1 for a normal export or 0x0 for the main module export.
- `flags`: An OR mask of valid flag values.
- `num_syms_funcs`: Number of function exports.
- `num_syms_vars`: Number of variable exports. Must be zero for kernel library export to user.
- `library nid`: NID of this library. Can be a random unique integer that is consistent. Importers will use this NID so it should only change when library changes are not backwards compatible.
- `library name`: Pointer to name of this exported library. For reference only and is not used in linking.
- `nid table`: Pointer to an array of 32-bit NIDs to export.
- `entry table`: Pointer to an array of data pointers corresponding to each exported NID (of the same index).

```
TODO
```

Should be set unless it is the main export.
In kernel modules only. Allow syscall imports.
Set for main export.

Note that since pointers are used, the .sceLib.ent section containing the export tables
can be relocated. The data pointed to (name string, NID array, and data array) are usually
stored in a section .sceExport.rodata. The order in the arrays (NID and data) is: function
exports followed by data exports followed by the unknown exports (the open toolchain should
define no such entries). The .sceExport.rodata section can also be relocated.
For all executables, a library with NID 0x00000000 and attributes 0x8000 exports the
module start and module stop functions along with a pointer to the module information
structure as a function export. The NIDs for these exports are as follows:

```
TODO
```

### Module Imports
Each module also has a list of imported libraries. The format of the import table is very similar to the format of the export table.

- `size`: Set to 0x34. There are other sized import tables that follow different formats. We will not support them for now.
- `version`: Set to 0x1.
- `flags`: Set to 0x0.
- `num syms funcs`: Number of function imports.
- `num syms vars`: Number of variable imports.
- `library nid`: NID of library to import. This is used to find what module to import

from and is the same NID as the library nid of the library from the exporting module.

```
TODO
```

- `library name`: Pointer to name of the imported library. For reference only and is not used for linking.
- `func nid table`: Pointer to an array of function NIDs to import.
- `func entry table`: Pointer to an array of stub functions to fill.
- `var nid table`: Pointer to an array of variable NIDs to import.
- `var entry table`: Pointer to an array of data pointers to write to.

The import tables are stored in the same way as export tables in a section .sceLib.stubs
which can be relocated. The data pointed to are usually found in .sceImport.rodata. The
function NIDs to import (for all imported libraries) is usually stored in section .sceFNID.rodata
and the corresponding stub functions are in .sceFStub.rodata. The stub functions are
found in .text and can be any function that is 12 bytes long (however, functions are usually
aligned to 16 bytes, which is fine too). Upon dynamic linking, the stub function is either
replaced with a jump to the user library or a syscall to an imported kernel module. The
suggested stub function is:
Imported variable NIDs can be stored in section .sceVNID.rodata and the data table in
.sceVNID.rodata.

```
TODO
```

### New Import Format
Newer firmware versions have modules with import tables of size 0x24 bytes instead of
the usual 0x34 byte structure defined above. The only difference is that some fields are
dropped. For reference, the new structure is defined below. However, support for it is
optional. Software can differentiate the two versions by looking at the size field. All import
entries must be the same size!

```
TODO
```