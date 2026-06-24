#ifndef BASIC_H
#define BASIC_H

#include <stdbool.h>

// ---------------------------------------------------------------------------
// MSX-BASIC argument-parsing primitives (implemented in basic.asm).
//
// These let a command handler parse its own arguments straight from the BASIC
// program text. They operate on a shared text pointer that is set up by the
// dispatcher before the handler runs. Calls that fail validation raise a BASIC
// error (Syntax error / Type mismatch) and do NOT return to the handler.
// ---------------------------------------------------------------------------

// Require the next program character to be `c`, then consume it.
// Typically used for the surrounding '(' ')' and the ',' separators.
extern void cmd_expect(char c) __z88dk_fastcall;

// Return the next program character (spaces skipped) WITHOUT consuming it.
// Useful for optional arguments, e.g. peeking for ')' to stop early.
extern char cmd_peek(void);

// Evaluate a numeric expression and return it as a 16-bit integer.
extern int cmd_get_int(void);

// Evaluate a numeric expression and return it as an 8-bit value (0-255).
extern unsigned char cmd_get_byte(void);

// Evaluate a string expression; returns a NUL-terminated copy held in a shared
// scratch buffer. The result is only valid until the next cmd_get_string call,
// so copy it if you need to keep more than one string at a time.
extern char *cmd_get_string(void);

// ---------------------------------------------------------------------------
// Returning values: parse a variable reference, then write back into it.
//
// cmd_get_var() parses the target variable (e.g. NAME$ or COUNT%) and returns
// its type: 2 = integer, 3 = string, 4 = single, 8 = double. Afterwards,
// cmd_set_string()/cmd_set_int() store a result into that variable.
// cmd_set_int only accepts integer variables (type 2, % suffix); any other
// type raises a BASIC "Type mismatch" error.
// ---------------------------------------------------------------------------
extern char cmd_get_var(void);
extern void cmd_set_string(const char *s) __z88dk_fastcall;
extern void cmd_set_int(int value) __z88dk_fastcall;

// Page in the FujiNet cartridge. Call this only from handlers that actually
// communicate with the device; it switches memory slots, so pure-software
// commands must not use it.
extern void fujinet_activate(void);

// Restore the primary-slot register to its state before fujinet_activate.
// Must be called before any cmd_set_int / cmd_set_string call, since BASIC
// variables may live in page 2 (0x8000-0xBFFF) which the cartridge occupies.
extern void fujinet_deactivate(void);

// ---------------------------------------------------------------------------
// Command handlers (dispatched by call_handler in basic.asm). Each handler
// parses its own arguments via the primitives above.
// ---------------------------------------------------------------------------
extern void basic_fujinet(void);
extern void basic_fnconfig(void);
extern bool basic_fngetdevice(void);
extern int  basic_nopen(void);
extern void basic_ninit(void);
extern void basic_nclose(void);
extern void basic_nstatus(void);
extern void basic_nread(void);
extern void basic_nreadnb(void);
extern void basic_nwrite(void);
extern void basic_njsonparse(void);
extern void basic_njsonquery(void);
extern void basic_nhttppost(void);

extern void basic_nhttpput(void);
extern void basic_nhttpdel(void);
extern void basic_nhttpaddhdr(void);
extern void basic_nhttpstarthdr(void);
extern void basic_nhttpendhdr(void);
extern void basic_nhttpmode(void);
extern void basic_naccept(void);


// WiFi
extern void basic_fwifienabled(void);
extern void basic_fwifistatus(void);
extern void basic_fwifiscan(void);
extern void basic_fwifiscanresult(void);
extern void basic_fgetwifissid(void);
extern void basic_fsetwifissid(void);

// Host Slots
extern void basic_floadhostslots(void);
extern void basic_fsavehostslots(void);
extern void basic_fgethostslot(void);
extern void basic_fsethostslot(void);
extern void basic_fmounthost(void);
extern void basic_fgethostprefix(void);
extern void basic_fsethostprefix(void);

// Device Slots
extern void basic_floaddevslots(void);
extern void basic_fsavedevslots(void);
extern void basic_fgetdevslothost(void);
extern void basic_fgetdevslotmode(void);
extern void basic_fgetdevslotfile(void);
extern void basic_fsetdevslothost(void);
extern void basic_fsetdevslotmode(void);
extern void basic_fsetdevslotfile(void);
extern void basic_fmount(void);
extern void basic_funmount(void);
extern void basic_fmountall(void);
extern void basic_fenabledev(void);
extern void basic_fdisabledev(void);
extern void basic_fsetfile(void);

// Directory
extern void basic_fopendir(void);
extern void basic_fopendirex(void);
extern void basic_fclosedir(void);
extern void basic_freaddir(void);
extern void basic_fsetdirpos(void);

// Boot
extern void basic_fsetbootcfg(void);
extern void basic_fsetbootmode(void);

// Hash
extern void basic_fhashclear(void);
extern void basic_fhashadd(void);
extern void basic_fhashcalc(void);
extern void basic_fhashdata(void);

#endif // BASIC_H
