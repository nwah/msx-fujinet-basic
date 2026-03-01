	extern	call_handler

	DB	"AB"		    ; identify as executable ROM
	DW	_main           ; main program address
	DW	call_handler    ; BASIC command handler
	DW	0		        ; DEVICE
	DW	0		        ; TEXT
	DW	0,0,0           ; Reserved
