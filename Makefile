ROM_CFILES = main.c basic.c fujinet-msx-ext.c
ROM_AFILES = basic.asm

fujinet-basic.rom: $(ROM_AFILES) $(ROM_CFILES)
	zcc +msx -subtype=rom -pragma-define:CRT_MSX_CUSTOM_HEADER=1 \
		-pragma-define:CRT_ORG_BSS=0xD700 \
		-m \
		-l/Users/nwah/fujinet/fujinet-basic-msx/_cache/fujinet-lib/fujinet-lib-experimental/r2r/msx/fujinet.msx.lib \
		-I/Users/nwah/fujinet/fujinet-basic-msx/_cache/fujinet-lib/fujinet-lib-experimental/include \
	    $^ -o $@
# zcc emits the initialized-data section as a separate file rather than
# appending it, but rom_init copies it into RAM from __ROMABLE_END_tail, which
# is the first address past the ROM image - so it has to be concatenated here.
# Then pad out to a full 16K cartridge.
	cat fujinet-basic_DATA.bin >> $@
	@$(MAKE) -s check-bss
	@perl -e 'open F, ">>", $$ARGV[0] or die; print F "\xFF" x (16384 - -s $$ARGV[0])' $@
	@echo "$@: $$(stat -f%z $@) bytes"

# BSS is reserved from BASIC by lowering HIMEM to CRT_ORG_BSS (see rom_init in
# basic.asm), so it has to stay clear of the work area a disk ROM may already
# have taken off the top of RAM before our INIT runs. 0xE800 leaves room for
# the usual one- or two-drive disk ROM.
BSS_CEILING = 0xE800

check-bss: fujinet-basic.map
	@end=$$(sed -n 's/^__BSS_END_head *= *\$$\([0-9A-Fa-f]*\).*/\1/p' $<); \
	if [ $$((0x$$end)) -gt $$(($(BSS_CEILING))) ]; then \
		echo "error: BSS ends at 0x$$end, past the $(BSS_CEILING) ceiling."; \
		echo "       Shrink the RAM buffers or lower CRT_ORG_BSS."; \
		exit 1; \
	fi

clean:
	rm -f *.map *.rom *.bin *.sym *.lis

.PHONY: clean check-bss
