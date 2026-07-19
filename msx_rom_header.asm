	extern	rom_init
	extern	call_handler
	extern	device_handler

	DB	"AB"		    ; identify as executable ROM
	DW	rom_init        ; INIT (runs at boot)
	DW	call_handler    ; STATEMENT (BASIC command handler)
	DW	device_handler  ; DEVICE (expanded "N:" device handler)
	DW	0		        ; TEXT (no auto-run BASIC program)
	DW	0,0,0           ; Reserved
