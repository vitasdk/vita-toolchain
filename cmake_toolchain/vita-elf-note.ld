/* Custom linker scripts and older linkers can opt into Vita ELF identification. */
SECTIONS
{
  .note.vitasdk 0 (INFO) : ALIGN(4)
  {
    LONG(8)
    LONG(4)
    LONG(1)
    ASCIZ "vitasdk"
    LONG(1)
  }
}
INSERT AFTER .bss;
