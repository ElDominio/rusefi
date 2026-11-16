// placeholder: EFI_EMBED_INI_MSD=FALSE in config/boards/f407-discovery/board.mk for f407-discovery
// real content skipped to avoid a "Disk full" failure when the INI is large (see above);
// this stub only exists so the simulator (which always wants EFI_EMBED_INI_MSD=TRUE) links
unsigned char ramdisk_image[] = { 0 };
