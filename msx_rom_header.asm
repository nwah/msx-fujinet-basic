	extern	call_handler

	DB	"AB"		    ; identify as executable ROM
	DW	_main           ; INIT (runs at boot)
	DW	call_handler    ; STATEMENT (BASIC command handler)
	DW	0		        ; DEVICE
	DW	0		        ; TEXT (no auto-run BASIC program)
	DW	0,0,0           ; Reserved
