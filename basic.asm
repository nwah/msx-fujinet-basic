CHPUT   equ 0x00A2
PROCNM  equ 0xFD89
CALBAS	equ	0x0159
ERRHAND equ 0x406F
FRMEVL  equ 0x4C64
GETBYT  equ 0x521C
FRESTR	equ	0x67D0
CHRGTR  equ 0x4666
VALTYP  equ 0xF663
USR     equ 0xF7F8

        extern _basic_fnconfig
        extern _basic_fngetdevice
        extern _basic_nopen

        public call_handler
        public banner

banner:
        push    hl
        ld      hl, banner_msg
        call    _puts
        pop     hl
        ret

banner_msg:
        defb "FujiNet BASIC",0

command_list:
       	defb "FNCONFIG",0
        defw fnconfig
        defb "FNGETDEVICE",0
        defw fngetdevice
        defb "NOPEN",0
        defw nopen
        defb 0

call_handler:
	    push hl
		ld  hl,command_list
_check_command:
		ld	de,PROCNM
_loop:	ld	a,(de)
		cp	(hl)
		jr	nz,_tonextcmd	; not equal
		inc	de
		inc	hl
		and	a
		jr	nz,_loop	; no end of instruction name, go checking
		ld	e,(hl)
		inc	hl
		ld	d,(hl)
		pop	hl		; routine address
		call _callde		; call routine
		and	a
		ret

_tonextcmd:
		ld	c,0ffh
		xor	a
		cpir			; skip to end of instruction name
		inc	hl
		inc	hl		; skip address
		cp	(hl)
		jr	nz, _check_command	; not end of table, go checking
		pop	hl
		scf
		ret

_callde:
		push de
		ret

activate_fujinet:
        ld      A,$D4   ; TODO: determine correct slot dynamically
        out     ($A8),A
        ret

fnconfig:
        call    activate_fujinet
        call    _basic_fnconfig
        ret

fngetdevice:
        call    activate_fujinet
        CALL	CHKCHAR
        DEFB	"("
        CALL    EVALINTPARAM
        PUSH    DE
        CALL	CHKCHAR
        DEFB	","
        CALL    EVALTXTPARAM
        PUSH    HL
        CALL    GETSTRPNT
        POP     HL
        PUSH    DE
        # DEC     HL     ; go back to closing " of string
        # LD      (HL),0 ; set to 0 for C-style nul-terminated str
        # INC     HL     ; forward to next token
        CALL	CHKCHAR
        DEFB	")"
        call    _basic_fngetdevice
        ret

nopen:
        call    activate_fujinet
        CALL	CHKCHAR
        DEFB	"("
        CALL    EVALTXTPARAM
        PUSH    HL
        CALL    GETSTRPNT
        POP     HL
        PUSH    DE
        DEC     HL     ; go back to closing " of string
        LD      (HL),0 ; set to 0 for C-style nul-terminated str
        INC     HL     ; forward to next token
        CALL	CHKCHAR
        DEFB	","
        CALL    EVALINTPARAM
        PUSH    DE
        CALL	CHKCHAR
        DEFB	","
        CALL    EVALINTPARAM
        PUSH    DE
        CALL	CHKCHAR
        DEFB	")"
        call    _basic_nopen
        pop     BC
        pop     BC
        xor     A

        # ld      A,1
        ret


;=========================================
; Based on examples by Nyyrikki and zPasi
; https://www.msx.org/wiki/CALL

_puts:
		ld  a, (hl)
        cp  0
        ret z
        call CHPUT
        inc hl
        jr  _puts

_putsn:
        ld  a, c
        cp  0
        ret z
        dec c
        ld  a, (hl)
        call CHPUT
        inc hl
        jr  _putsn

GETSTRPNT:
; OUT:
; DE = String Address
; B  = Lenght
        LD      HL,(USR)
        LD      B,(HL)
        INC     HL
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        # EX      DE,HL
        RET

EVALINTPARAM:
       	LD	    IX,GETBYT
       	CALL	CALBAS		    ; Evaluate expression
        RET

EVALTXTPARAM:
    	LD	    IX,FRMEVL
    	CALL	CALBAS		    ; Evaluate expression
        LD      A,(VALTYP)
        CP      3               ; Text type?
        JP      NZ,TYPE_MISMATCH
        PUSH	HL
        LD	    IX,FRESTR       ; Free the temporary string
        CALL	CALBAS
        POP 	HL
        RET

CHKCHAR:
        CALL	GETPREVCHAR	; Get previous basic char
        EX	(SP),HL
    	CP	(HL) 	        ; Check if good char
    	JR	NZ,SYNTAX_ERROR	; No, Syntax error
    	INC	HL
    	EX	(SP),HL
    	INC	HL		; Get next basic char

GETPREVCHAR:
    	DEC	HL
    	LD	IX,CHRGTR
    	JP      CALBAS

TYPE_MISMATCH:
        LD      E,13
        DB      1

SYNTAX_ERROR:
        LD      E,2
    	LD	IX,ERRHAND	; Call the Basic error handler
    	JP	CALBAS
