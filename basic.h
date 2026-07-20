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

// Raise a BASIC error; does not return. Uses the interpreter's own numbering
// (19 = "Device I/O error", 2 = "Syntax error", 13 = "Type mismatch").
extern void cmd_error(unsigned char code) __z88dk_fastcall;

// Page in the FujiNet cartridge. Call this only from handlers that actually
// communicate with the device; it switches memory slots, so pure-software
// commands must not use it.
extern void fujinet_activate(void);

// Restore the primary-slot register to its state before fujinet_activate.
// Must be called before any cmd_set_int / cmd_set_string call, since BASIC
// variables may live in page 2 (0x8000-0xBFFF) which the cartridge occupies.
extern void fujinet_deactivate(void);

// Install the boot-time version banner hook. Call once from INIT; see the
// comment above install_boot_banner_hook in basic.asm for why this isn't
// just a direct print.
extern void install_boot_banner_hook(void);

// ---------------------------------------------------------------------------
// Command handlers (dispatched by call_handler in basic.asm). Each handler
// parses its own arguments via the primitives above.
// ---------------------------------------------------------------------------
extern void basic_fujinet(void);
extern void basic_fujinet_boot(void);
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

// App Keys
extern void basic_fsetappkey(void);
extern void basic_freadappkey(void);
extern void basic_fwriteappkey(void);
// Base64
extern void basic_fb64encin(void);
extern void basic_fb64enccompute(void);
extern void basic_fb64enclen(void);
extern void basic_fb64encout(void);
extern void basic_fb64decin(void);
extern void basic_fb64deccompute(void);
extern void basic_fb64declen(void);
extern void basic_fb64decout(void);
// Hash
extern void basic_fhashclear(void);
extern void basic_fhashadd(void);
extern void basic_fhashcalc(void);
extern void basic_fhashdata(void);
extern void basic_ncd(void);


// ---------------------------------------------------------------------------
// Expanded device ("N:") handlers, dispatched by device_handler in basic.asm
// when BASIC performs I/O on a file whose device name we claimed. See the
// comment above device_handler for the protocol, and the ndev_* block in
// basic.c for what each request means.
// ---------------------------------------------------------------------------

// The caller's registers, spilled on entry and reloaded on exit. Handlers read
// their arguments from here and write results back into it. Field order must
// stay in sync with the stores in device_handler.
//
// `carry` is the flag BASIC reads back: sequential input sets it to 1 to mean
// "end of file", which is how the interpreter raises "Input past end" itself.
struct dev_regs_t {
  unsigned char a;
  unsigned char carry;
  unsigned char c, b;
  unsigned char e, d;
  unsigned char l, h;
  unsigned char err;   // nonzero: raise this BASIC error instead of returning
};
extern struct dev_regs_t dev_regs;

// Network unit (1-8) taken from the device name at the time BASIC asked us
// whether we handle it: "N:" and "N1:" are unit 1, "N2:" unit 2, and so on.
extern unsigned char ndev_unit;

// Path prepended to the 8.3 filename to form the devicespec. Set by
// CALL NCD; see basic_ncd in basic.c for why it is needed.
extern char ndev_prefix[];

extern void ndev_open(void);
extern void ndev_close(void);
extern void ndev_random(void);
extern void ndev_output(void);
extern void ndev_input(void);
extern void ndev_loc(void);
extern void ndev_lof(void);
extern void ndev_eof(void);
extern void ndev_fpos(void);
extern void ndev_backup(void);

// ---------------------------------------------------------------------------
// Printer redirection (see the printer block in basic.c and basic.asm).
//
// Installed once at boot; from then on LPRINT, LLIST and PRINT# to an open
// "LPT:" go to the FujiNet printer device instead of the Centronics port.
// ---------------------------------------------------------------------------

// Patch the H.LPTO / H.LPTS system hooks. Call from INIT only: it reads the
// cartridge's own slot number, which is only meaningful while page 1 is us.
extern void install_printer_hooks(void);

// The character handed over by the H.LPTO stub, read by lpt_out.
extern unsigned char lpt_char;

// RAM home for the hook stubs; install_printer_hooks copies them here.
extern unsigned char lpt_stub[];

// Buffer one character of printer output; flushes on end of line or when full.
extern void lpt_out(void);

// Send buffered printer output to FujiNet now.
extern void lpt_flush(void);

#endif // BASIC_H
