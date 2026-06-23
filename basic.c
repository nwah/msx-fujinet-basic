#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <conio.h>
#include "fujinet-fuji.h"
#include "fujinet-network.h"
#include "basic.h"

AdapterConfigExtended c;
char buf[256];
char buf2[256];  // second scratch buffer for commands with two string inputs + one output

// Shared scratch used by the parsing primitives in basic.asm. Defining these
// in C places them in RAM (the ROM crt can't write to ROM):
//   txtptr  - the BASIC program text pointer, threaded across every C call.
//   strbuf  - destination for cmd_get_string().
//   varptr  - storage address of the variable parsed by cmd_get_var().
//   vartype - that variable's type (2/3/4/8).
unsigned char *txtptr;
char strbuf[256];
unsigned char *varptr;
unsigned char vartype;
unsigned char saved_slot;

// CALL FNCONFIG
void basic_fnconfig(void) {
  fujinet_activate();
  cputs("FujiNet Config\n\n");

  fuji_get_adapter_config_extended(&c);
  fujinet_deactivate();

  cprintf(" %8s %s\n","SSID",c.ssid);
  cprintf(" %8s %s\n","HOSTNAME",c.hostname);
  cprintf(" %8s %u.%u.%u.%u\n","IP",c.localIP[0],c.localIP[1],c.localIP[2],c.localIP[3]);
  cprintf(" %8s %u.%u.%u.%u\n","NETMASK",c.netmask[0],c.netmask[1],c.netmask[2],c.netmask[3]);
  cprintf(" %8s %u.%u.%u.%u\n","DNS",c.dnsIP[0],c.dnsIP[1],c.dnsIP[2],c.dnsIP[3]);
  cprintf(" %8s %02x:%02x:%02x:%02x:%02x:%02x\n","MAC",c.macAddress[0],c.macAddress[1],c.macAddress[2],c.macAddress[3],c.macAddress[4],c.macAddress[5]);
  cprintf(" %8s %02x:%02x:%02x:%02x:%02x:%02x\n","BSSID",c.bssid[0],c.bssid[1],c.bssid[2],c.bssid[3],c.bssid[4],c.bssid[5]);
  cprintf(" %8s %s\n","FN VER",c.fn_version);
}

// CALL FNGETDEVICE(device_slot, name$)
//   Reads the device's filename and assigns it to the string variable name$.
bool basic_fngetdevice(void) {
  cmd_expect('(');
  int ds = cmd_get_int();
  cmd_expect(',');
  cmd_get_var();         // target string variable (e.g. NAME$)
  cmd_expect(')');

  fujinet_activate();
  fuji_get_device_filename(ds, buf);
  fujinet_deactivate();
  cmd_set_string(buf);   // write the filename back into name$
  return true;
}

// CALL NOPEN(devicespec$, mode, translation)
int basic_nopen(void) {
  cmd_expect('(');
  char *devicespec = cmd_get_string();
  cmd_expect(',');
  int mode = cmd_get_int();
  cmd_expect(',');
  int trans = cmd_get_int();
  cmd_expect(')');

  fujinet_activate();
  int ret = network_open(devicespec, (uint8_t)mode, (uint8_t)trans);
  fujinet_deactivate();
  return ret;
}

// CALL NINIT
void basic_ninit(void) {
  fujinet_activate();
  network_init();
  fujinet_deactivate();
}

// CALL NCLOSE(devicespec$)
void basic_nclose(void) {
  cmd_expect('(');
  char *devicespec = cmd_get_string();
  cmd_expect(')');

  fujinet_activate();
  network_close(devicespec);
  fujinet_deactivate();
}

// CALL NSTATUS(devicespec$, bw%, conn%, err%)
//   Queries the network device status and stores bytes-waiting, connection
//   state, and error code in the three integer variables.
void basic_nstatus(void) {
  cmd_expect('(');
  char *devicespec = cmd_get_string();
  cmd_expect(',');
  cmd_get_var();   // bw%
  unsigned char *bw_ptr  = varptr;
  unsigned char  bw_type = vartype;
  cmd_expect(',');
  cmd_get_var();  // conn%
  unsigned char *conn_ptr  = varptr;
  unsigned char  conn_type = vartype;
  cmd_expect(',');
  cmd_get_var();
  cmd_expect(')');

  uint16_t bw;
  uint8_t  conn, err;
  fujinet_activate();
  network_status(devicespec, &bw, &conn, &err);
  fujinet_deactivate();                         // restore page 2 to RAM before writing variables

  cmd_set_int((int)err);                        // err% = error byte

  varptr = conn_ptr; vartype = conn_type;
  cmd_set_int((int)conn);                       // conn% = connection status

  varptr = bw_ptr; vartype = bw_type;
  cmd_set_int((int)bw);                         // bw% = bytes waiting
}

// CALL NREAD(devicespec$, len, result$, S%)
//   Blocking read of up to len bytes into result$; S% receives the byte count.
void basic_nread(void) {
  cmd_expect('(');
  char *devicespec = cmd_get_string();      // devicespec lives in strbuf
  cmd_expect(',');
  int len = cmd_get_int();
  cmd_expect(',');
  cmd_get_var();                            // result$
  unsigned char *str_ptr  = varptr;
  unsigned char  str_type = vartype;
  cmd_expect(',');
  cmd_get_var();                            // S% (varptr/vartype left pointing here)
  cmd_expect(')');

  if (len > 255) len = 255;
  fujinet_activate();
  int16_t bytes = network_read(devicespec, buf, (uint16_t)len);
  if (bytes < 0) bytes = 0;
  buf[bytes] = '\0';
  fujinet_deactivate();

  cmd_set_int((int)bytes);                  // S% = bytes read

  varptr = str_ptr; vartype = str_type;
  cmd_set_string(buf);                      // result$ = data
}

// CALL NREADNB(devicespec$, len, result$, S%)
//   Non-blocking read of up to len bytes into result$; S% receives the byte count.
void basic_nreadnb(void) {
  cmd_expect('(');
  char *devicespec = cmd_get_string();      // devicespec lives in strbuf
  cmd_expect(',');
  int len = cmd_get_int();
  cmd_expect(',');
  cmd_get_var();                            // result$
  unsigned char *str_ptr  = varptr;
  unsigned char  str_type = vartype;
  cmd_expect(',');
  cmd_get_var();                            // S% (varptr/vartype left pointing here)
  cmd_expect(')');

  if (len > 255) len = 255;
  fujinet_activate();
  int16_t bytes = network_read_nb(devicespec, buf, (uint16_t)len);
  if (bytes < 0) bytes = 0;
  buf[bytes] = '\0';
  fujinet_deactivate();

  cmd_set_int((int)bytes);                  // S% = bytes read

  varptr = str_ptr; vartype = str_type;
  cmd_set_string(buf);                      // result$ = data
}

// CALL NWRITE(devicespec$, data$)
void basic_nwrite(void) {
  cmd_expect('(');
  char *devicespec = cmd_get_string();
  strcpy(buf, devicespec);                  // save before next cmd_get_string overwrites strbuf
  cmd_expect(',');
  char *data = cmd_get_string();
  cmd_expect(')');

  fujinet_activate();
  network_write(buf, data, (uint16_t)strlen(data));
  fujinet_deactivate();
}

// CALL NJSONPARSE(devicespec$)
void basic_njsonparse(void) {
  cmd_expect('(');
  char *devicespec = cmd_get_string();
  cmd_expect(')');

  fujinet_activate();
  network_json_parse(devicespec);
  fujinet_deactivate();
}

// CALL NJSONQUERY(devicespec$, query$, result$, S%)
//   Issues a JSON path query; result$ receives the value string, S% the byte count.
void basic_njsonquery(void) {
  cmd_expect('(');
  char *devicespec = cmd_get_string();
  strcpy(buf, devicespec);                  // save devicespec; query will overwrite strbuf
  cmd_expect(',');
  cmd_get_string();                         // query string now in strbuf
  cmd_expect(',');
  cmd_get_var();                            // result$
  unsigned char *str_ptr  = varptr;
  unsigned char  str_type = vartype;
  cmd_expect(',');
  cmd_get_var();                            // S% (varptr/vartype left pointing here)
  cmd_expect(')');

  fujinet_activate();
  // buf = devicespec, strbuf = query, buf2 = output
  int16_t bytes = network_json_query(buf, strbuf, buf2);
  if (bytes < 0) bytes = 0;
  fujinet_deactivate();

  cmd_set_int((int)bytes);                  // S% = bytes read

  varptr = str_ptr; vartype = str_type;
  cmd_set_string(buf2);                     // result$ = query result
}

// CALL NHTTPPOST(devicespec$, data$)
void basic_nhttppost(void) {
  cmd_expect('(');
  char *devicespec = cmd_get_string();
  strcpy(buf, devicespec);
  cmd_expect(',');
  char *data = cmd_get_string();
  cmd_expect(')');

  fujinet_activate();
  network_http_post(buf, data);
  fujinet_deactivate();
}

// CALL NHTTPPUT(devicespec$, data$)
void basic_nhttpput(void) {
  cmd_expect('(');
  char *devicespec = cmd_get_string();
  strcpy(buf, devicespec);
  cmd_expect(',');
  char *data = cmd_get_string();
  cmd_expect(')');

  fujinet_activate();
  network_http_put(buf, data);
  fujinet_deactivate();
}

// CALL NHTTPDEL(devicespec$, trans)
//   Opens a DELETE request (connection must be read/closed by caller).
void basic_nhttpdel(void) {
  cmd_expect('(');
  char *devicespec = cmd_get_string();
  cmd_expect(',');
  int trans = cmd_get_int();
  cmd_expect(')');

  fujinet_activate();
  network_http_delete(devicespec, (uint8_t)trans);
  fujinet_deactivate();
}

// CALL NHTTPADDHDR(devicespec$, header$)
void basic_nhttpaddhdr(void) {
  cmd_expect('(');
  char *devicespec = cmd_get_string();
  strcpy(buf, devicespec);
  cmd_expect(',');
  char *header = cmd_get_string();
  cmd_expect(')');

  fujinet_activate();
  network_http_add_header(buf, header);
  fujinet_deactivate();
}

// CALL NHTTPSTARTHDR(devicespec$)
void basic_nhttpstarthdr(void) {
  cmd_expect('(');
  char *devicespec = cmd_get_string();
  cmd_expect(')');

  fujinet_activate();
  network_http_start_add_headers(devicespec);
  fujinet_deactivate();
}

// CALL NHTTPENDHDR(devicespec$)
void basic_nhttpendhdr(void) {
  cmd_expect('(');
  char *devicespec = cmd_get_string();
  cmd_expect(')');

  fujinet_activate();
  network_http_end_add_headers(devicespec);
  fujinet_deactivate();
}

// CALL NHTTPMODE(devicespec$, mode)
//   Sets the HTTP channel mode (0=body, 1=collect headers, 2=get headers, 3=set headers, 4=post data).
void basic_nhttpmode(void) {
  cmd_expect('(');
  char *devicespec = cmd_get_string();
  cmd_expect(',');
  int mode = cmd_get_int();
  cmd_expect(')');

  fujinet_activate();
  network_http_set_channel_mode(devicespec, mode);
  fujinet_deactivate();
}

// CALL NACCEPT(devicespec$)
void basic_naccept(void) {
  cmd_expect('(');
  char *devicespec = cmd_get_string();
  cmd_expect(')');

  fujinet_activate();
  network_accept(devicespec);
  fujinet_deactivate();
}

// CALL FNADD(a, b, result)
//   Computes a + b and stores the sum in the numeric variable result.
//   Pure software: it must not page in the FujiNet cartridge.
void basic_fnadd(void) {
  cmd_expect('(');
  int a = cmd_get_int();
  cmd_expect(',');
  int b = cmd_get_int();
  cmd_expect(',');
  cmd_get_var();         // target numeric variable (e.g. R)
  cmd_expect(')');

  cmd_set_int((uint16_t)(a + b));
}

// CALL FNSTATUS(result)
//   Stores a single-byte status value (0-255) in the numeric variable result.
//   Pure software: like FNADD it must not page in the FujiNet cartridge.
//   Exercises the 1-byte integer return path through cmd_set_int().
void basic_fnstatus(void) {
  cmd_expect('(');
  cmd_get_var();         // target numeric variable (e.g. S)
  cmd_expect(')');

  cmd_set_int(6502);   // demo status code
}

// CALL FNHELLO(result$)
//   Assigns the constant string "HELLO" to the string variable result$.
//   Lets you verify the string-return path without any FujiNet hardware.
void basic_fnhello(void) {
  cmd_expect('(');
  cmd_get_var();         // target string variable (e.g. A$)
  cmd_expect(')');

  cmd_set_string("HELLO");
}
