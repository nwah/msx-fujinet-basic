	extern	call_handler
	extern  banner

	DB	"AB"		    ; identify as executable ROM
	DW	_main           ; main program address
	DW	call_handler    ; BASIC command handler
	DW	0		        ; DEVICE
	DW	message      	        ; TEXT
	DW	0,0,0           ; Reserved

message:
    DB  $91,"\"Hi\""
