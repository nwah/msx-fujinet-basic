ROM_CFILES = main.c basic.c fujinet-msx-ext.c
ROM_AFILES = basic.asm

fujinet-basic.rom: $(ROM_AFILES) $(ROM_CFILES)
	zcc +msx -subtype=rom -pragma-define:CRT_MSX_CUSTOM_HEADER=1 \
		-m \
		-l/Users/nwah/fujinet/fujinet-basic-msx/_cache/fujinet-lib/fujinet-lib-experimental/r2r/msx/fujinet.msx.lib \
		-I/Users/nwah/fujinet/fujinet-basic-msx/_cache/fujinet-lib/fujinet-lib-experimental/include \
	    $^ -o $@

clean:
	rm *.map *.rom *.bin *.sym *.lis > /dev/null
