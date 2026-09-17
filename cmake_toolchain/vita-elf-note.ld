/* Custom linker scripts and older linkers can opt into Vita ELF identification. */
SECTIONS
{
  .note.vitasdk 0 (INFO) : ALIGN(4)
  {
    LONG(8)
    LONG(4)
    LONG(1)
    BYTE(0x56) BYTE(0x69) BYTE(0x74) BYTE(0x61)
    BYTE(0x53) BYTE(0x44) BYTE(0x4b) BYTE(0)
    LONG(1)
  }
}
INSERT AFTER .bss;
