; FujiNet BASIC: command dispatch + MSX-BASIC parsing primitives.
;
; The argument parsers below expose the MSX-BASIC interpreter's expression
; evaluator to C. MSX-BASIC keeps the program text pointer in HL, but C calls
; clobber HL (and most registers), so we stash it in the RAM variable _txtptr
; and thread it through every primitive. That lets a command handler - argument
; parsing and all - be written entirely in C.

PROCNM  equ 0xFD89
DEVICE  equ 0xFD99      ; device id (0-3) BASIC is currently talking to
PTRFIL  equ 0xF864      ; active I/O channel, 0 = keyboard/screen
CALBAS  equ 0x0159
ERRHAND equ 0x406F
FRMEVL  equ 0x4C64
FRMQNT  equ 0x542F      ; evaluate expression -> 16-bit integer in DE
GETBYT  equ 0x521C      ; evaluate expression -> 8-bit value in A (and E)
FRESTR  equ 0x67D0
CHRGTR  equ 0x4666
PTRGET  equ 0x5EA4      ; parse a variable reference -> DE = storage address
STRINI  equ 0x6627      ; allocate string space (A = length) -> DSCTMP
VALTYP  equ 0xF663
SUBFLG  equ 0xF6A5      ; 0 = simple variable / array element
DSCTMP  equ 0xF698      ; temporary string descriptor [len][addr_lo][addr_hi]

        ; C command handlers (one per BASIC command)
        extern _basic_fujinet
        extern _basic_fujinet_boot
        extern _basic_fnconfig
        extern _basic_fngetdevice
        extern _basic_nopen
        extern _basic_ninit
        extern _basic_nclose
        extern _basic_nstatus
        extern _basic_nread
        extern _basic_nreadnb
        extern _basic_nwrite
        extern _basic_njsonparse
        extern _basic_njsonquery
        extern _basic_nhttppost

        extern _basic_nhttpput
        extern _basic_nhttpdel
        extern _basic_nhttpaddhdr
        extern _basic_nhttpstarthdr
        extern _basic_nhttpendhdr
        extern _basic_nhttpmode
        extern _basic_naccept


        ; WiFi
        extern _basic_fwifienabled
        extern _basic_fwifistatus
        extern _basic_fwifiscan
        extern _basic_fwifiscanresult
        extern _basic_fgetwifissid
        extern _basic_fsetwifissid

        ; Host Slots
        extern _basic_floadhostslots
        extern _basic_fsavehostslots
        extern _basic_fgethostslot
        extern _basic_fsethostslot
        extern _basic_fmounthost
        extern _basic_fgethostprefix
        extern _basic_fsethostprefix

        ; Device Slots
        extern _basic_floaddevslots
        extern _basic_fsavedevslots
        extern _basic_fgetdevslothost
        extern _basic_fgetdevslotmode
        extern _basic_fgetdevslotfile
        extern _basic_fsetdevslothost
        extern _basic_fsetdevslotmode
        extern _basic_fsetdevslotfile
        extern _basic_fmount
        extern _basic_funmount
        extern _basic_fmountall
        extern _basic_fenabledev
        extern _basic_fdisabledev
        extern _basic_fsetfile

        ; Directory
        extern _basic_fopendir
        extern _basic_fopendirex
        extern _basic_fclosedir
        extern _basic_freaddir
        extern _basic_fsetdirpos

        ; Boot
        extern _basic_fsetbootcfg
        extern _basic_fsetbootmode

        ; App Keys
        extern _basic_fsetappkey
        extern _basic_freadappkey
        extern _basic_fwriteappkey
        ; Base64
        extern _basic_fb64encin
        extern _basic_fb64enccompute
        extern _basic_fb64enclen
        extern _basic_fb64encout
        extern _basic_fb64decin
        extern _basic_fb64deccompute
        extern _basic_fb64declen
        extern _basic_fb64decout
        ; Hash
        extern _basic_fhashclear
        extern _basic_fhashadd
        extern _basic_fhashcalc
        extern _basic_fhashdata
        extern _basic_nprefix


        ; Expanded-device ("N:") handlers, one per BASIC device request
        extern _ndev_open
        extern _ndev_close
        extern _ndev_random
        extern _ndev_output
        extern _ndev_input
        extern _ndev_loc
        extern _ndev_lof
        extern _ndev_eof
        extern _ndev_fpos
        extern _ndev_backup

        ; RAM scratch defined in C (so the ROM crt places it in RAM)
        extern _txtptr
        extern _strbuf
        extern _varptr
        extern _vartype
        extern _saved_slot
        extern _dev_regs
        extern _ndev_unit

        ; Parsing primitives exported to C
        public _cmd_expect
        public _cmd_peek
        public _cmd_get_int
        public _cmd_get_byte
        public _cmd_get_string
        public _cmd_get_var
        public _cmd_set_string
        public _cmd_set_int
        public _cmd_error
        public _fujinet_activate
        public _fujinet_deactivate
        public _install_boot_banner_hook

        public call_handler
        public device_handler

;======================================================================
; Command table: name (NUL-terminated) followed by C handler address.
command_list:
        defb "FUJINET",0
        defw _basic_fujinet
        defb "FNCONFIG",0
        defw _basic_fnconfig
        defb "FNGETDEVICE",0
        defw _basic_fngetdevice
        defb "NOPEN",0
        defw _basic_nopen
        defb "NINIT",0
        defw _basic_ninit
        defb "NCLOSE",0
        defw _basic_nclose
        defb "NSTATUS",0
        defw _basic_nstatus
        defb "NREAD",0
        defw _basic_nread
        defb "NREADNB",0
        defw _basic_nreadnb
        defb "NWRITE",0
        defw _basic_nwrite
        defb "NJSONPARSE",0
        defw _basic_njsonparse
        defb "NJSONQUERY",0
        defw _basic_njsonquery
        defb "NHTTPPOST",0
        defw _basic_nhttppost
        defb "NHTTPPUT",0
        defw _basic_nhttpput
        defb "NHTTPDEL",0
        defw _basic_nhttpdel
        defb "NHTTPADDHDR",0
        defw _basic_nhttpaddhdr
        defb "NHTTPSTARTHDR",0
        defw _basic_nhttpstarthdr
        defb "NHTTPENDHDR",0
        defw _basic_nhttpendhdr
        defb "NHTTPMODE",0
        defw _basic_nhttpmode
        defb "NACCEPT",0
        defw _basic_naccept

        ; WiFi
        defb "FWIFIENABLED",0
        defw _basic_fwifienabled
        defb "FWIFISTATUS",0
        defw _basic_fwifistatus
        defb "FWIFISCAN",0
        defw _basic_fwifiscan
        defb "FWIFISCANRESULT",0
        defw _basic_fwifiscanresult
        defb "FGETWIFISSID",0
        defw _basic_fgetwifissid
        defb "FSETWIFISSID",0
        defw _basic_fsetwifissid
        ; Host Slots
        defb "FLOADHOSTSLOTS",0
        defw _basic_floadhostslots
        defb "FSAVEHOSTSLOTS",0
        defw _basic_fsavehostslots
        defb "FGETHOSTSLOT",0
        defw _basic_fgethostslot
        defb "FSETHOSTSLOT",0
        defw _basic_fsethostslot
        defb "FMOUNTHOST",0
        defw _basic_fmounthost
        defb "FGETHOSTPREFIX",0
        defw _basic_fgethostprefix
        defb "FSETHOSTPREFIX",0
        defw _basic_fsethostprefix
        ; Device Slots
        defb "FLOADDEVSLOTS",0
        defw _basic_floaddevslots
        defb "FSAVEDEVSLOTS",0
        defw _basic_fsavedevslots
        defb "FGETDEVSLOTHOST",0
        defw _basic_fgetdevslothost
        defb "FGETDEVSLOTMODE",0
        defw _basic_fgetdevslotmode
        defb "FGETDEVSLOTFILE",0
        defw _basic_fgetdevslotfile
        defb "FSETDEVSLOTHOST",0
        defw _basic_fsetdevslothost
        defb "FSETDEVSLOTMODE",0
        defw _basic_fsetdevslotmode
        defb "FSETDEVSLOTFILE",0
        defw _basic_fsetdevslotfile
        defb "FMOUNT",0
        defw _basic_fmount
        defb "FUNMOUNT",0
        defw _basic_funmount
        defb "FMOUNTALL",0
        defw _basic_fmountall
        defb "FENABLEDEV",0
        defw _basic_fenabledev
        defb "FDISABLEDEV",0
        defw _basic_fdisabledev
        defb "FSETFILE",0
        defw _basic_fsetfile
        ; Directory
        defb "FOPENDIR",0
        defw _basic_fopendir
        defb "FOPENDIREX",0
        defw _basic_fopendirex
        defb "FCLOSEDIR",0
        defw _basic_fclosedir
        defb "FREADDIR",0
        defw _basic_freaddir
        defb "FSETDIRPOS",0
        defw _basic_fsetdirpos
        ; Boot
        defb "FSETBOOTCFG",0
        defw _basic_fsetbootcfg
        defb "FSETBOOTMODE",0
        defw _basic_fsetbootmode
        ; App Keys
        defb "FSETAPPKEY",0
        defw _basic_fsetappkey
        defb "FREADAPPKEY",0
        defw _basic_freadappkey
        defb "FWRITEAPPKEY",0
        defw _basic_fwriteappkey
        ; Base64
        defb "FB64ENCIN",0
        defw _basic_fb64encin
        defb "FB64ENCCOMPUTE",0
        defw _basic_fb64enccompute
        defb "FB64ENCLEN",0
        defw _basic_fb64enclen
        defb "FB64ENCOUT",0
        defw _basic_fb64encout
        defb "FB64DECIN",0
        defw _basic_fb64decin
        defb "FB64DECCOMPUTE",0
        defw _basic_fb64deccompute
        defb "FB64DECLEN",0
        defw _basic_fb64declen
        defb "FB64DECOUT",0
        defw _basic_fb64decout
        ; Hash
        defb "FHASHCLEAR",0
        defw _basic_fhashclear
        defb "FHASHADD",0
        defw _basic_fhashadd
        defb "FHASHCALC",0
        defw _basic_fhashcalc
        defb "FHASHDATA",0
        defw _basic_fhashdata
        ; Expanded device
        defb "NPREFIX",0
        defw _basic_nprefix

        defb 0

;----------------------------------------------------------------------
; call_handler: invoked by the BASIC expansion-statement hook.
;   In:  HL -> program text just after the command name
;        PROCNM holds the upper-cased command name
;   Out: CY clear  -> command handled
;        CY set    -> not one of ours (HL left untouched)
call_handler:
        ld      (_txtptr),hl        ; hand the text pointer to the C primitives
        ld      hl,command_list
_check_command:
        ld      de,PROCNM
_loop:
        ld      a,(de)
        cp      (hl)
        jr      nz,_tonextcmd
        inc     de
        inc     hl
        and     a
        jr      nz,_loop            ; not at end of name yet
        ; matched: read the handler address that follows the name
        ld      e,(hl)
        inc     hl
        ld      d,(hl)              ; DE = handler address
        ex      de,hl               ; HL = handler address
        call    _call_hl            ; run the C handler
        ld      hl,(_txtptr)        ; resume BASIC after the consumed tokens
        or      a                   ; CY clear = handled
        ret

_tonextcmd:
        ld      c,0ffh
        xor     a
        cpir                        ; skip to end of this name
        inc     hl
        inc     hl                  ; skip handler address
        cp      (hl)
        jr      nz,_check_command   ; not end of table, keep looking
        ld      hl,(_txtptr)        ; not ours: restore text pointer
        scf
        ret

_call_hl:
        jp      (hl)

;======================================================================
; Expanded device ("N:") support.
;
; MSX-BASIC resolves the device part of a filename - the text before the
; ':' in LOAD "N:...", OPEN "N:..." etc - by first trying its own built-in
; devices (CAS:, CRT:, LPT:, GRP:, MEM:, and disk drive letters). If none
; match, it upper-cases the name into PROCNM, sets A=0FFh, and calls the
; DEVICE entry (offset 6) of every cartridge header that has a non-zero
; one, lowest slot first. We claim the name here and BASIC then routes all
; I/O on that file back through this same entry.
;
; Two distinct calls arrive at device_handler:
;
;   A = 0FFh  "do you handle the device named in PROCNM?"
;             -> A = device id (0-3), CY clear   if yes
;             -> CY set                          if not ours
;
;   A = 0..18 an I/O request on a file we claimed. DEVICE (0FD99h) holds
;             the device id from the name check, so one cartridge can serve
;             up to 4 device names. The codes are spaced by 2 because BASIC
;             uses them as a byte offset into a table of JR instructions,
;             which is exactly what dev_table below is.
;
; Register conventions per request code are NOT documented in the MSX2
; Technical Handbook (it lists only the code meanings), so rather than
; guess which register carries the data byte / result for each op, the
; dispatcher spills AF/BC/DE/HL into _dev_regs before entering C and
; reloads them afterwards. A handler reads its inputs and writes its
; result through that struct; working out which field each op actually
; uses is a per-op job for an emulator debugger. See ndev_* in basic.c.
;----------------------------------------------------------------------
device_handler:
        cp      0FFh
        jp      z,dev_name_check    ; out of JR range now

;----------------------------------------------------------------------
; I/O request: A = request code, DEVICE (0FD99h) = our device id.
        cp      dev_table_end-dev_table
        ret     nc                  ; code past the end of the table: ignore
        ld      (_dev_regs+0),a     ; spill the caller's registers for C
        ld      (_dev_regs+2),bc
        ld      (_dev_regs+4),de
        ld      (_dev_regs+6),hl
        ld      e,a                 ; stash the table offset BEFORE clobbering A:
        ld      d,0                 ; the request code doubles as the offset
        xor     a
        ld      (_dev_regs+1),a     ; default carry-out to clear for every request
        ld      (_dev_regs+8),a     ; and "no error raised"
        ld      hl,dev_table
        add     hl,de
        jp      (hl)

dev_table:
        jr      dev_open            ; 0  OPEN
        jr      dev_close           ; 2  CLOSE
        jr      dev_random          ; 4  random access
        jr      dev_output          ; 6  sequential output
        jr      dev_input           ; 8  sequential input
        jr      dev_loc             ; 10 LOC()
        jr      dev_lof             ; 12 LOF()
        jr      dev_eof             ; 14 EOF()
        jr      dev_fpos            ; 16 FPOS()
        jr      dev_backup          ; 18 backup character
dev_table_end:

dev_open:
        ld      hl,_ndev_open
        jr      dev_call
dev_close:
        ld      hl,_ndev_close
        jr      dev_call
dev_random:
        ld      hl,_ndev_random
        jr      dev_call
dev_output:
        ld      hl,_ndev_output
        jr      dev_call
dev_input:
        ld      hl,_ndev_input
        jr      dev_call
dev_loc:
        ld      hl,_ndev_loc
        jr      dev_call
dev_lof:
        ld      hl,_ndev_lof
        jr      dev_call
dev_eof:
        ld      hl,_ndev_eof
        jr      dev_call
dev_fpos:
        ld      hl,_ndev_fpos
        jr      dev_call
dev_backup:
        ld      hl,_ndev_backup
        ; fall through

; Enter the C handler in HL, then hand the (possibly updated) _dev_regs
; back to BASIC in real registers.
dev_call:
        push    ix
        push    iy
        call    _call_hl
        pop     iy
        pop     ix
        ld      a,(_dev_regs+8)     ; handler asked us to fail the operation?
        and     a
        jr      nz,dev_raise
        ld      bc,(_dev_regs+2)
        ld      de,(_dev_regs+4)
        ld      hl,(_dev_regs+6)
        ld      a,(_dev_regs+1)     ; carry-out byte (sequential input: 1 = EOF)
        rrca                        ; bit 0 -> CY; only RRCA touches the flags,
        ld      a,(_dev_regs+0)     ; and the LDs below/above leave them alone
        ret

;----------------------------------------------------------------------
; Raise a BASIC error on a handler's behalf.
;
; A handler must NOT call cmd_error itself: it would longjmp out of a live
; C frame in the middle of dispatch, and since the compiler cannot know the
; call never returns, the handler would carry on and report success on a
; channel that was never opened - BASIC then retries and the error repeats
; forever. Instead a handler stores the code in dev_regs.err and returns
; normally, and we raise it here with the C frame already unwound.
;
; PTRFIL is cleared first. BASIC's error routine prints through PTRFIL, so
; leaving our own dying channel installed would route the error message
; straight back into the device it is complaining about.
dev_raise:
        ld      e,a                 ; E = BASIC error code
        ld      hl,0
        ld      (PTRFIL),hl         ; detach the channel before BASIC prints
        ld      ix,ERRHAND
        jp      CALBAS

;----------------------------------------------------------------------
; Name check: accept "N" (unit 1) and "N1".."N8", mirroring how the MSX
; serial ROM claims COM0..COM9 from a single device id. The unit digit is
; stashed in _ndev_unit for the OPEN that follows, so all eight units share
; device id 0 - BASIC allows only 4 ids per cartridge, not enough to give
; each unit one of its own.
dev_name_check:
        ld      hl,PROCNM
        ld      a,(hl)
        cp      04Eh                ; 'N'
        jr      nz,dev_not_ours
        inc     hl
        ld      a,(hl)
        and     a                   ; bare "N:"?
        jr      z,dev_unit_default
        sub     031h                ; '1'
        jr      c,dev_not_ours      ; not a digit in 1-8
        cp      8
        jr      nc,dev_not_ours
        inc     a                   ; A = unit 1-8
        ld      c,a
        inc     hl
        ld      a,(hl)
        and     a                   ; the digit must end the name
        jr      nz,dev_not_ours
        ld      a,c
        jr      dev_unit_set
dev_unit_default:
        ld      a,1                 ; bare "N:" means unit 1
dev_unit_set:
        ld      (_ndev_unit),a
        xor     a                   ; device id 0, CY clear = ours
        ret
dev_not_ours:
        scf
        ret

; void fujinet_activate(void)
; Page in the FujiNet cartridge. Saves the current primary-slot register so
; fujinet_deactivate can restore it. Only commands that talk to the device
; need this; pure-software commands (e.g. FNADD) must NOT switch slots.
_fujinet_activate:
        in      a,(0A8h)            ; save current slot configuration
        ld      (_saved_slot),a
        and     0CFh                ; clear page 2 slot bits (5-4)
        or      010h                ; set page 2 to slot 1
        out     (0A8h),a
        ret

; void fujinet_deactivate(void)
; Restore the primary-slot register to the value saved by fujinet_activate.
; Must be called before writing back to BASIC variables, since those may live
; in page 2 (0x8000-0xBFFF) which is occupied by the cartridge while active.
_fujinet_deactivate:
        ld      a,(_saved_slot)
        out     (0A8h),a
        ret

; void install_boot_banner_hook(void)
; Patch the H.READ system hook (RAM hook table entry at 0xFF07) so the
; version banner prints once, right between BASIC's "nnnnn Bytes free" line
; and its "Ok" prompt - the same spot real disk BASIC ROMs print their
; banner. H.READ is called by BASIC's own cold-start every time it returns
; to the command loop (so also after every subsequent command), which is
; too often; the hook self-clears back to a plain RET the first time it
; runs so the banner only ever appears once.
;
; Printing directly from our own INIT is too early: INIT runs before
; BASIC's cold-start clears the screen and prints its own banner, so
; anything printed there gets wiped before the user ever sees it.
;
; By the time H.READ fires, page 1 (0x4000-0x7FFF) has been switched away
; from our cartridge's slot back to BASIC's own RAM - confirmed by tracing
; this in an emulator debugger, since a plain "JP our_code" hook here
; crashes (it jumps to the right *address*, but that address now belongs
; to whatever's mapped into page 1 at the time, not our ROM). So the hook
; itself must be a CALLF (RST 30h) - same 5-byte "F7,slot,addrlo,addrhi,C9"
; encoding used by real disk ROMs for their own self-hooks - which pages
; our slot in, calls us, and restores the caller's mapping before the
; trailing RET. The slot byte is read from our own primary-slot config at
; INIT time (page 1 is guaranteed to be us then), matching how
; fujinet_activate reads/restores port 0xA8 elsewhere in this file. This
; assumes a simple, non-expanded slot, which every real cartridge edge
; connector is in practice.
HREAD   equ     0FF07h

_install_boot_banner_hook:
        ld      hl,_boot_hook_template
        ld      de,HREAD
        ld      bc,5
        ldir                        ; F7 00 <entry-lo> <entry-hi> C9
        in      a,(0A8h)
        rrca                        ; page-1 slot bits (3-2) down to bits 1-0
        rrca
        and     03h
        ld      (HREAD+1),a         ; patch the slot byte, now that it's in RAM
        ret

_boot_hook_template:
        defb    0F7h                ; RST 30 (CALLF)
        defb    0                   ; slot byte placeholder, patched above
        defw    _boot_hook_entry
        defb    0C9h                ; RET - CALLF returns here, completing the hook

_boot_hook_entry:
        push    af
        push    bc
        push    de
        push    hl
        push    ix
        push    iy
        ld      hl,HREAD            ; self-clear: restore the default 5-byte
        ld      b,5                 ; RET hook so this only ever fires once
_clear_hread:
        ld      (hl),0C9h
        inc     hl
        djnz    _clear_hread
        call    _basic_fujinet_boot
        pop     iy
        pop     ix
        pop     hl
        pop     de
        pop     bc
        pop     af
        ret

;======================================================================
; Parsing primitives (callable from C).
; Each loads the shared text pointer into HL, calls into BASIC, and writes
; the advanced pointer back to _txtptr.
;======================================================================

;----------------------------------------------------------------------
; void cmd_expect(char c)            __z88dk_fastcall  (c in L)
; Require the next program char to be c and consume it, else Syntax error.
_cmd_expect:
        ld      e,l                 ; E = expected char (CHRGTR preserves DE)
        ld      hl,(_txtptr)
        dec     hl
        ld      ix,CHRGTR
        call    CALBAS              ; HL -> current char, A = char
        cp      e
        jp      nz,_syntax_error
        ld      ix,CHRGTR
        call    CALBAS              ; advance past the matched char
        ld      (_txtptr),hl
        ret

;----------------------------------------------------------------------
; char cmd_peek(void)
; Return the next program char (spaces skipped) WITHOUT consuming it.
; Useful for optional arguments, e.g. peeking for ')' vs ','.
_cmd_peek:
        ld      hl,(_txtptr)
        dec     hl
        ld      ix,CHRGTR
        call    CALBAS              ; HL -> current char, A = char
        ld      (_txtptr),hl        ; normalise past any skipped spaces
        ld      l,a
        ld      h,0
        ret

;----------------------------------------------------------------------
; int cmd_get_int(void)
; Evaluate a numeric expression, return it as a 16-bit integer in HL.
_cmd_get_int:
        ld      hl,(_txtptr)
        dec     hl
        ld      ix,CHRGTR
        call    CALBAS              ; A = current char (primes the evaluator)
        ld      ix,FRMQNT
        call    CALBAS              ; DE = value, HL advanced
        ld      (_txtptr),hl
        ex      de,hl
        ret

;----------------------------------------------------------------------
; unsigned char cmd_get_byte(void)
; Evaluate a numeric expression, return it as an 8-bit value in L.
_cmd_get_byte:
        ld      hl,(_txtptr)
        dec     hl
        ld      ix,CHRGTR
        call    CALBAS              ; A = current char (primes the evaluator)
        ld      ix,GETBYT
        call    CALBAS              ; A = E = value, HL advanced
        ld      (_txtptr),hl
        ld      l,a
        ld      h,0
        ret

;----------------------------------------------------------------------
; char *cmd_get_string(void)
; Evaluate a string expression and return a NUL-terminated copy in _strbuf
; (valid until the next cmd_get_string call). Copying out avoids mutating
; the BASIC program text. Length is capped at 255 bytes by the BASIC string
; descriptor format.
_cmd_get_string:
        ld      hl,(_txtptr)
        dec     hl
        ld      ix,CHRGTR
        call    CALBAS              ; A = current char (primes the evaluator)
        ld      ix,FRMEVL
        call    CALBAS              ; evaluate -> FAC, HL advanced
        ld      (_txtptr),hl
        ld      a,(VALTYP)
        cp      3                   ; string type?
        jp      nz,_type_mismatch
        ld      ix,FRESTR
        call    CALBAS              ; HL -> string descriptor
        ld      b,(hl)              ; B = length
        inc     hl
        ld      e,(hl)
        inc     hl
        ld      d,(hl)              ; DE = string data address
        ld      hl,_strbuf
_cgs_copy:
        ld      a,b
        or      a
        jr      z,_cgs_done
        ld      a,(de)
        ld      (hl),a
        inc     de
        inc     hl
        dec     b
        jr      _cgs_copy
_cgs_done:
        ld      (hl),0              ; NUL terminate
        ld      hl,_strbuf
        ret

;----------------------------------------------------------------------
; char cmd_get_var(void)
; Parse a variable reference (the assignment target) at the text pointer.
; Remembers its storage address (_varptr) and type (_vartype), and returns
; the type: 2 = integer, 3 = string, 4 = single, 8 = double.
_cmd_get_var:
        xor     a
        ld      (SUBFLG),a          ; simple variable / array element
        ld      hl,(_txtptr)
        dec     hl
        ld      ix,CHRGTR
        call    CALBAS              ; A = current char (PTRGET needs it primed)
        ld      ix,PTRGET
        call    CALBAS              ; DE = storage address, HL advanced
        ld      (_txtptr),hl
        ld      (_varptr),de
        ld      a,(VALTYP)
        ld      (_vartype),a
        ld      l,a
        ld      h,0
        ret

;----------------------------------------------------------------------
; void cmd_set_string(const char *s)   __z88dk_fastcall  (s in HL)
; Assign a NUL-terminated string to the (string) variable parsed by
; cmd_get_var. Allocates fresh BASIC string space so each result is
; independent. Raises Type mismatch if the target is not a string.
_cmd_set_string:
        ld      a,(_vartype)
        cp      3
        jp      nz,_type_mismatch
        ; measure length, capped at 255 (HL = source)
        ld      e,l
        ld      d,h                 ; DE = scan pointer
        ld      c,0                 ; C = length
_css_len:
        ld      a,(de)
        or      a
        jr      z,_css_got
        ld      a,c
        cp      255
        jr      z,_css_got
        inc     de
        inc     c
        jr      _css_len
_css_got:
        push    hl                  ; save source pointer
        push    bc                  ; save length (in C)
        ld      a,c
        ld      ix,STRINI
        call    CALBAS              ; reserve A bytes; HL -> DSCTMP descriptor
        inc     hl
        ld      e,(hl)
        inc     hl
        ld      d,(hl)              ; DE = reserved string-space address
        pop     bc                  ; C = length
        pop     hl                  ; HL = source
        ld      b,0
        ld      a,c
        or      a
        jr      z,_css_assign
        ldir                        ; copy C bytes (HL) -> (DE)
_css_assign:
        ld      hl,DSCTMP
        ld      de,(_varptr)
        ld      bc,3
        ldir                        ; descriptor -> variable storage
        ret

;----------------------------------------------------------------------
; void cmd_set_int(int value)   __z88dk_fastcall  (value in HL)
; Store a 16-bit integer directly into the integer variable (type 2,
; % suffix) parsed by cmd_get_var. Raises Type mismatch for any other
; variable type (string, single, double).
_cmd_set_int:
        ld      a,(_vartype)
        cp      2
        jp      nz,_type_mismatch
        ld      de,(_varptr)
        ex      de,hl               ; HL = varptr, DE = value
        ld      (hl),e              ; low byte
        inc     hl
        ld      (hl),d              ; high byte
        ret

;======================================================================
; Error exits
;======================================================================

;----------------------------------------------------------------------
; void cmd_error(char code)   __z88dk_fastcall  (code in L)
; Raise a BASIC error and do NOT return - ERRHAND longjmps back into the
; interpreter. Codes are the interpreter's own numbering, e.g. 19 =
; "Device I/O error", 2 = "Syntax error", 13 = "Type mismatch".
_cmd_error:
        ld      e,l
        ld      ix,ERRHAND
        jp      CALBAS

_type_mismatch:
        ld      e,13
        db      1                   ; "LD BC,nn" swallows the next "LD E,2"
_syntax_error:
        ld      e,2
        ld      ix,ERRHAND
        jp      CALBAS
