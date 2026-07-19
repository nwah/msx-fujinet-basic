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

#define FUJINET_BASIC_VERSION "0.2.2"

// CALL FUJINET
void basic_fujinet(void) {
  cputs("FujiNet BASIC v" FUJINET_BASIC_VERSION "\r\n");
}

// Printed automatically at boot via the H.READ hook (see
// install_boot_banner_hook in basic.asm). Needs a leading CRLF since it
// lands right after BASIC's "Bytes free" line, on the same line.
void basic_fujinet_boot(void) {
  cputs("\r\nFujiNet BASIC v" FUJINET_BASIC_VERSION "\r\n");
}

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



// ============================================================
// Static globals shared by the commands below
// ============================================================
static NetConfig  net_config;
static HostSlot   host_slots[8];
static DeviceSlot device_slots[8];
static SSIDInfo   ssid_info;
static unsigned long b64_len;
static const uint8_t hash_bin_sizes[4] = {16, 20, 32, 64}; // MD5, SHA1, SHA256, SHA512

// ============================================================
// Base64 commands
// ============================================================

// CALL FB64ENCIN(data$)
void basic_fb64encin(void) {
  cmd_expect('(');
  cmd_get_string();
  cmd_expect(')');
  uint16_t len = (uint16_t)strlen(strbuf);
  fujinet_activate();
  fuji_base64_encode_input(strbuf, len);
  fujinet_deactivate();
}

// CALL FB64ENCCOMPUTE
void basic_fb64enccompute(void) {
  fujinet_activate();
  fuji_base64_encode_compute();
  fujinet_deactivate();
}

// CALL FB64ENCLEN(S%)
void basic_fb64enclen(void) {
  cmd_expect('(');
  cmd_get_var();
  cmd_expect(')');
  b64_len = 0;
  fujinet_activate();
  fuji_base64_encode_length(&b64_len);
  fujinet_deactivate();
  cmd_set_int((int)b64_len);
}

// CALL FB64ENCOUT(result$)
void basic_fb64encout(void) {
  cmd_expect('(');
  cmd_get_var();
  cmd_expect(')');
  memset(buf2, 0, sizeof(buf2));
  fujinet_activate();
  fuji_base64_encode_output(buf2, 255);
  fujinet_deactivate();
  cmd_set_string(buf2);
}

// CALL FB64DECIN(data$)
void basic_fb64decin(void) {
  cmd_expect('(');
  cmd_get_string();
  cmd_expect(')');
  uint16_t len = (uint16_t)strlen(strbuf);
  fujinet_activate();
  fuji_base64_decode_input(strbuf, len);
  fujinet_deactivate();
}

// CALL FB64DECCOMPUTE
void basic_fb64deccompute(void) {
  fujinet_activate();
  fuji_base64_decode_compute();
  fujinet_deactivate();
}

// CALL FB64DECLEN(S%)
void basic_fb64declen(void) {
  cmd_expect('(');
  cmd_get_var();
  cmd_expect(')');
  b64_len = 0;
  fujinet_activate();
  fuji_base64_decode_length(&b64_len);
  fujinet_deactivate();
  cmd_set_int((int)b64_len);
}

// CALL FB64DECOUT(result$)
void basic_fb64decout(void) {
  cmd_expect('(');
  cmd_get_var();
  cmd_expect(')');
  memset(buf2, 0, sizeof(buf2));
  fujinet_activate();
  fuji_base64_decode_output(buf2, 255);
  fujinet_deactivate();
  cmd_set_string(buf2);
}

// ============================================================
// WiFi commands
// ============================================================

// CALL FWIFIENABLED(S%)
void basic_fwifienabled(void) {
  cmd_expect('(');
  cmd_get_var();
  cmd_expect(')');
  fujinet_activate();
  bool enabled = fuji_get_wifi_enabled();
  fujinet_deactivate();
  cmd_set_int(enabled ? 1 : 0);
}

// CALL FWIFISTATUS(S%)
void basic_fwifistatus(void) {
  cmd_expect('(');
  cmd_get_var();
  cmd_expect(')');
  fujinet_activate();
  uint8_t wst = 0;
  fuji_get_wifi_status(&wst);
  fujinet_deactivate();
  cmd_set_int((int)wst);
}

// CALL FWIFISCAN(S%)
void basic_fwifiscan(void) {
  cmd_expect('(');
  cmd_get_var();
  cmd_expect(')');
  fujinet_activate();
  uint8_t cnt = 0;
  fuji_scan_for_networks(&cnt);
  fujinet_deactivate();
  cmd_set_int((int)cnt);
}

// CALL FWIFISCANRESULT(n, ssid$, rssi%)
void basic_fwifiscanresult(void) {
  cmd_expect('(');
  int n = cmd_get_int();
  cmd_expect(',');
  cmd_get_var();  // ssid$
  unsigned char *ssid_ptr  = varptr;
  unsigned char  ssid_type = vartype;
  cmd_expect(',');
  cmd_get_var();  // rssi% (varptr/vartype left pointing here)
  cmd_expect(')');
  fujinet_activate();
  fuji_get_scan_result((uint8_t)n, &ssid_info);
  fujinet_deactivate();
  cmd_set_int((int)(signed char)ssid_info.rssi);
  varptr = ssid_ptr; vartype = ssid_type;
  cmd_set_string(ssid_info.ssid);
}

// CALL FGETWIFISSID(ssid$)
void basic_fgetwifissid(void) {
  cmd_expect('(');
  cmd_get_var();
  cmd_expect(')');
  fujinet_activate();
  fuji_get_ssid(&net_config);
  fujinet_deactivate();
  cmd_set_string(net_config.ssid);
}

// CALL FSETWIFISSID(ssid$, password$)
void basic_fsetwifissid(void) {
  cmd_expect('(');
  cmd_get_string();
  strncpy(net_config.ssid, strbuf, SSID_MAXLEN-1);
  net_config.ssid[SSID_MAXLEN-1] = '\0';
  cmd_expect(',');
  cmd_get_string();
  strncpy(net_config.password, strbuf, MAX_PASSWORD_LEN-1);
  net_config.password[MAX_PASSWORD_LEN-1] = '\0';
  cmd_expect(')');
  fujinet_activate();
  fuji_set_ssid(&net_config);
  fujinet_deactivate();
}

// ============================================================
// Host Slot commands
// ============================================================

// CALL FLOADHOSTSLOTS
void basic_floadhostslots(void) {
  fujinet_activate();
  fuji_get_host_slots(host_slots, 8);
  fujinet_deactivate();
}

// CALL FSAVEHOSTSLOTS
void basic_fsavehostslots(void) {
  fujinet_activate();
  fuji_put_host_slots(host_slots, 8);
  fujinet_deactivate();
}

// CALL FGETHOSTSLOT(slot, name$)  -- pure software
void basic_fgethostslot(void) {
  cmd_expect('(');
  int slot = cmd_get_int();
  if (slot < 0) slot = 0;
  if (slot > 7) slot = 7;
  cmd_expect(',');
  cmd_get_var();
  cmd_expect(')');
  cmd_set_string((char*)host_slots[slot]);
}

// CALL FSETHOSTSLOT(slot, name$)  -- pure software
void basic_fsethostslot(void) {
  cmd_expect('(');
  int slot = cmd_get_int();
  if (slot < 0) slot = 0;
  if (slot > 7) slot = 7;
  cmd_expect(',');
  cmd_get_string();
  cmd_expect(')');
  strncpy((char*)host_slots[slot], strbuf, 31);
  ((char*)host_slots[slot])[31] = '\0';
}

// CALL FMOUNTHOST(hs)
void basic_fmounthost(void) {
  cmd_expect('(');
  int hs = cmd_get_int();
  cmd_expect(')');
  fujinet_activate();
  fuji_mount_host_slot((uint8_t)hs);
  fujinet_deactivate();
}

// CALL FGETHOSTPREFIX(hs, prefix$)
void basic_fgethostprefix(void) {
  cmd_expect('(');
  int hs = cmd_get_int();
  cmd_expect(',');
  cmd_get_var();
  cmd_expect(')');
  fujinet_activate();
  fuji_get_host_prefix((uint8_t)hs, buf);
  fujinet_deactivate();
  cmd_set_string(buf);
}

// CALL FSETHOSTPREFIX(hs, prefix$)
void basic_fsethostprefix(void) {
  cmd_expect('(');
  int hs = cmd_get_int();
  cmd_expect(',');
  cmd_get_string();
  cmd_expect(')');
  fujinet_activate();
  fuji_set_host_prefix((uint8_t)hs, strbuf);
  fujinet_deactivate();
}

// ============================================================
// Device Slot commands
// ============================================================

// CALL FLOADDEVSLOTS
void basic_floaddevslots(void) {
  fujinet_activate();
  fuji_get_device_slots(device_slots, 8);
  fujinet_deactivate();
}

// CALL FSAVEDEVSLOTS
void basic_fsavedevslots(void) {
  fujinet_activate();
  fuji_put_device_slots(device_slots, 8);
  fujinet_deactivate();
}

// CALL FGETDEVSLOTHOST(slot, S%)  -- pure software
void basic_fgetdevslothost(void) {
  cmd_expect('(');
  int slot = cmd_get_int();
  cmd_expect(',');
  cmd_get_var();
  cmd_expect(')');
  cmd_set_int((int)device_slots[slot].hostSlot);
}

// CALL FGETDEVSLOTMODE(slot, S%)  -- pure software
void basic_fgetdevslotmode(void) {
  cmd_expect('(');
  int slot = cmd_get_int();
  cmd_expect(',');
  cmd_get_var();
  cmd_expect(')');
  cmd_set_int((int)device_slots[slot].mode);
}

// CALL FGETDEVSLOTFILE(slot, file$)  -- pure software
void basic_fgetdevslotfile(void) {
  cmd_expect('(');
  int slot = cmd_get_int();
  cmd_expect(',');
  cmd_get_var();
  cmd_expect(')');
  cmd_set_string((char*)device_slots[slot].file);
}

// CALL FSETDEVSLOTHOST(slot, host)  -- pure software
void basic_fsetdevslothost(void) {
  cmd_expect('(');
  int slot = cmd_get_int();
  cmd_expect(',');
  int host = cmd_get_int();
  cmd_expect(')');
  device_slots[slot].hostSlot = (uint8_t)host;
}

// CALL FSETDEVSLOTMODE(slot, mode)  -- pure software
void basic_fsetdevslotmode(void) {
  cmd_expect('(');
  int slot = cmd_get_int();
  cmd_expect(',');
  int mode = cmd_get_int();
  cmd_expect(')');
  device_slots[slot].mode = (uint8_t)mode;
}

// CALL FSETDEVSLOTFILE(slot, file$)  -- pure software
void basic_fsetdevslotfile(void) {
  cmd_expect('(');
  int slot = cmd_get_int();
  cmd_expect(',');
  cmd_get_string();
  cmd_expect(')');
  strncpy((char*)device_slots[slot].file, strbuf, FILE_MAXLEN-1);
  device_slots[slot].file[FILE_MAXLEN-1] = '\0';
}

// CALL FMOUNT(ds, mode)
void basic_fmount(void) {
  cmd_expect('(');
  int ds = cmd_get_int();
  cmd_expect(',');
  int mode = cmd_get_int();
  cmd_expect(')');
  fujinet_activate();
  fuji_mount_disk_image((uint8_t)ds, (uint8_t)mode);
  fujinet_deactivate();
}

// CALL FUNMOUNT(ds)
void basic_funmount(void) {
  cmd_expect('(');
  int ds = cmd_get_int();
  cmd_expect(')');
  fujinet_activate();
  fuji_unmount_disk_image((uint8_t)ds);
  fujinet_deactivate();
}

// CALL FMOUNTALL
void basic_fmountall(void) {
  fujinet_activate();
  fuji_mount_all();
  fujinet_deactivate();
}

// CALL FENABLEDEV(d)
void basic_fenabledev(void) {
  cmd_expect('(');
  int d = cmd_get_int();
  cmd_expect(')');
  fujinet_activate();
  fuji_enable_device((uint8_t)d);
  fujinet_deactivate();
}

// CALL FDISABLEDEV(d)
void basic_fdisabledev(void) {
  cmd_expect('(');
  int d = cmd_get_int();
  cmd_expect(')');
  fujinet_activate();
  fuji_disable_device((uint8_t)d);
  fujinet_deactivate();
}

// CALL FSETFILE(ds, hs, mode, file$)
void basic_fsetfile(void) {
  cmd_expect('(');
  int ds = cmd_get_int();
  cmd_expect(',');
  int hs = cmd_get_int();
  cmd_expect(',');
  int mode = cmd_get_int();
  cmd_expect(',');
  cmd_get_string();
  cmd_expect(')');
  fujinet_activate();
  fuji_set_device_filename((uint8_t)mode, (uint8_t)hs, (uint8_t)ds, strbuf);
  fujinet_deactivate();
}

// ============================================================
// Directory commands
// ============================================================

// CALL FOPENDIR(hs, path$)
void basic_fopendir(void) {
  cmd_expect('(');
  int hs = cmd_get_int();
  cmd_expect(',');
  cmd_get_string();
  cmd_expect(')');
  fujinet_activate();
  fuji_open_directory_filter((uint8_t)hs, strbuf, NULL);
  fujinet_deactivate();
}

// CALL FOPENDIREX(hs, path$, filter$)
void basic_fopendirex(void) {
  cmd_expect('(');
  int hs = cmd_get_int();
  cmd_expect(',');
  cmd_get_string();
  strcpy(buf, strbuf);    // save path before filter overwrites strbuf
  cmd_expect(',');
  cmd_get_string();
  cmd_expect(')');
  fujinet_activate();
  fuji_open_directory_filter((uint8_t)hs, buf, strbuf);
  fujinet_deactivate();
}

// CALL FCLOSEDIR
void basic_fclosedir(void) {
  fujinet_activate();
  fuji_close_directory();
  fujinet_deactivate();
}

// CALL FREADDIR(result$)
void basic_freaddir(void) {
  cmd_expect('(');
  cmd_get_var();
  cmd_expect(')');
  fujinet_activate();
  fuji_read_directory(255, 0, buf);
  fujinet_deactivate();
  cmd_set_string(buf);
}

// CALL FSETDIRPOS(pos)
void basic_fsetdirpos(void) {
  cmd_expect('(');
  int pos = cmd_get_int();
  cmd_expect(')');
  fujinet_activate();
  fuji_set_directory_position((uint16_t)pos);
  fujinet_deactivate();
}

// ============================================================
// Boot commands
// ============================================================

// CALL FSETBOOTCFG(toggle)
void basic_fsetbootcfg(void) {
  cmd_expect('(');
  int toggle = cmd_get_int();
  cmd_expect(')');
  fujinet_activate();
  fuji_set_boot_config((uint8_t)toggle);
  fujinet_deactivate();
}

// CALL FSETBOOTMODE(mode)
void basic_fsetbootmode(void) {
  cmd_expect('(');
  int mode = cmd_get_int();
  cmd_expect(')');
  fujinet_activate();
  fuji_set_boot_mode((uint8_t)mode);
  fujinet_deactivate();
}

// ============================================================
// App Key commands
// ============================================================

// CALL FSETAPPKEY(creator_id, app_id)
//   Set credentials for subsequent app key operations. Must be called first.
void basic_fsetappkey(void) {
  cmd_expect('(');
  int creator_id = cmd_get_int();
  cmd_expect(',');
  int app_id = cmd_get_int();
  cmd_expect(')');
  fujinet_activate();
  fuji_set_appkey_details((uint16_t)creator_id, (uint8_t)app_id, DEFAULT);
  fujinet_deactivate();
}

// CALL FREADAPPKEY(key_id, result$, S%)
//   Read app key key_id into result$; S% receives the byte count.
void basic_freadappkey(void) {
  cmd_expect('(');
  int key_id = cmd_get_int();
  cmd_expect(',');
  cmd_get_var();                            // result$
  unsigned char *str_ptr  = varptr;
  unsigned char  str_type = vartype;
  cmd_expect(',');
  cmd_get_var();                            // S% (varptr/vartype left pointing here)
  cmd_expect(')');
  uint16_t count = 0;
  memset(buf, 0, sizeof(buf));              // pre-zero so strlen gives actual length
  fujinet_activate();
  fuji_read_appkey((uint8_t)key_id, &count, (uint8_t *)buf);
  fujinet_deactivate();
  count = (uint16_t)strlen(buf);            // trim to actual content
  if (count > 255) count = 255;
  cmd_set_int((int)count);                  // S% = bytes read
  varptr = str_ptr; vartype = str_type;
  cmd_set_string("hello");                      // result$ = key data
}

// CALL FWRITEAPPKEY(key_id, data$)
//   Write data$ to app key key_id (max 64 bytes).
void basic_fwriteappkey(void) {
  cmd_expect('(');
  int key_id = cmd_get_int();
  cmd_expect(',');
  cmd_get_string();                         // data in strbuf
  cmd_expect(')');
  uint16_t len = (uint16_t)strlen(strbuf);
  if (len > MAX_APPKEY_LEN) len = MAX_APPKEY_LEN;
  fujinet_activate();
  fuji_write_appkey((uint8_t)key_id, len, (uint8_t *)strbuf);
  fujinet_deactivate();
}

// ============================================================
// Hash commands
// ============================================================

// CALL FHASHCLEAR
void basic_fhashclear(void) {
  fujinet_activate();
  fuji_hash_clear();
  fujinet_deactivate();
}

// CALL FHASHADD(data$)
void basic_fhashadd(void) {
  cmd_expect('(');
  cmd_get_string();
  cmd_expect(')');
  uint16_t len = (uint16_t)strlen(strbuf);
  fujinet_activate();
  fuji_hash_add((uint8_t *)strbuf, len);
  fujinet_deactivate();
}

// CALL FHASHCALC(type, hex, discard, result$)
//   Compute hash of accumulated data. type: 0=MD5 1=SHA1 2=SHA256 3=SHA512
//   hex: 1=hex string (recommended), 0=binary. discard: 1=free data after.
void basic_fhashcalc(void) {
  cmd_expect('(');
  int type    = cmd_get_int();
  cmd_expect(',');
  int as_hex  = cmd_get_int();
  cmd_expect(',');
  int discard = cmd_get_int();
  cmd_expect(',');
  cmd_get_var();
  cmd_expect(')');
  memset(buf2, 0, sizeof(buf2));
  fujinet_activate();
  fuji_hash_calculate((hash_alg_t)type, (bool)as_hex, (bool)discard, (uint8_t *)buf2);
  fujinet_deactivate();
  if (!as_hex && type >= 0 && type < 4)
    buf2[hash_bin_sizes[type]] = '\0';
  cmd_set_string(buf2);
}

// CALL FHASHDATA(type, data$, hex, result$)
//   Single-shot hash of data$. type: 0=MD5 1=SHA1 2=SHA256 3=SHA512
//   hex: 1=hex string (recommended), 0=binary.
void basic_fhashdata(void) {
  cmd_expect('(');
  int type = cmd_get_int();
  cmd_expect(',');
  cmd_get_string();                          // data$ -> strbuf
  uint16_t len = (uint16_t)strlen(strbuf);
  cmd_expect(',');
  int as_hex = cmd_get_int();
  cmd_expect(',');
  cmd_get_var();
  cmd_expect(')');
  memset(buf2, 0, sizeof(buf2));
  fujinet_activate();
  fuji_hash_data((hash_alg_t)type, (uint8_t *)strbuf, len, (bool)as_hex, (uint8_t *)buf2);
  fujinet_deactivate();
  if (!as_hex && type >= 0 && type < 4)
    buf2[hash_bin_sizes[type]] = '\0';
  cmd_set_string(buf2);
}

// ---------------------------------------------------------------------------
// Expanded device: "N:"
//
// BASIC resolves the device part of a filename (the text before the ':') by
// trying its built-ins first; when none match it upper-cases the name into
// PROCNM and offers it to each cartridge's DEVICE entry. We claim "N" and
// "N1".."N8", after which every I/O request on that file comes back to
// device_handler in basic.asm, which calls one of the ndev_* handlers here.
//
// BASIC expresses LOAD, SAVE, RUN, MERGE, OPEN, CLOSE, PRINT#, INPUT#,
// LINE INPUT# and EOF()/LOC()/LOF() in terms of these primitives, so
// implementing open/close/input/output/eof is enough to make
//
//     SAVE "N:PROG.BAS"        LOAD "N:PROG.BAS"        RUN "N:PROG.BAS"
//
// work. The disk-only statements (FILES, KILL, NAME, COPY) are served by a
// disk ROM directly and never reach device expansion.
//
// Register conventions per request code are not in the official docs; they
// were taken from the MSX system ROM sources (the MEM: device in subrom.mac
// and the COM: device in serial.mac), which are the two reference
// implementations of this interface.
// ---------------------------------------------------------------------------

// MSX BASIC work area. All of these live in page 3, which is always RAM, so
// they stay readable while the FujiNet cartridge is paged into page 2.
#define MSX_FILNAM ((const char *)0xF866)          // filename, 8+3, space padded
#define MSX_PTRFIL (*(unsigned char **)0xF864)     // active I/O channel, 0 = none
#define MSX_DAC    (*(int *)0xF7F8)                // DAC+2: integer function result
#define MSX_VALTYP (*(unsigned char *)0xF663)      // type of that result, 2 = integer

// Offsets into BASIC's I/O channel control block (the block HL points at).
#define FL_MOD 0                                   // open mode
#define FL_LSA 3                                   // backup character
#define FL_BPS 6                                   // position in the channel's buffer
#define FL_FLG 7                                   // channel flags; bit 7 = tokenized BASIC file

// Open modes BASIC passes in E.
#define FMODE_INPUT  1
#define FMODE_OUTPUT 2
#define FMODE_RANDOM 4
#define FMODE_APPEND 8

#define ERR_ILLEGAL_FUNCTION 5
#define ERR_DEVICE_IO        19

// The FujiNet ACCESS_MODE for append; fujinet-network.h has no constant for
// it (see fileAccessMode_t in the firmware's network-protocol/Protocol.h).
#define OPEN_MODE_APPEND 9

// How many times to poll status for bytes before calling a read finished.
// network_status also pumps the socket firmware-side, so polling is what
// actually moves data, not just what observes it.
#define NDEV_POLL_TRIES 200

unsigned char ndev_unit = 1;          // set by the name check in basic.asm
char ndev_prefix[192];                // set by CALL NCD

struct dev_regs_t dev_regs;

// One open "N:" file at a time. BASIC would allow several (MAXFILES), but the
// channel block is only guaranteed big enough for its own bookkeeping, so the
// buffering state lives here rather than inside the block.
static char          ndev_spec[224];  // "N1:" + prefix + filename
static unsigned char ndev_buf[128];   // read-ahead / write-behind buffer
static uint16_t      ndev_len;        // bytes valid in ndev_buf
static uint16_t      ndev_pos;        // next byte to hand to BASIC (read side)
static unsigned char ndev_ateof;
static unsigned char ndev_mode;
static unsigned char ndev_backchar;   // pushed-back byte, 0 = none
static unsigned char ndev_ascii;      // input is text, so translate line endings
static unsigned char ndev_lastcr;     // last byte handed over was a CR

// The I/O channel control block BASIC passed in HL.
static unsigned char *ndev_chan(void) {
  return (unsigned char *)(((unsigned int)dev_regs.h << 8) | dev_regs.l);
}

// Build the devicespec from the unit, the prefix, and BASIC's 8.3 FILNAM.
// BASIC only ever gives a device an 8.3 name, so the scheme/host/path has to
// come from the prefix - that is what CALL NCD is for.
static void ndev_make_spec(void) {
  const char *f = MSX_FILNAM;
  char *p = ndev_spec;
  unsigned char i;

  *p++ = 'N';
  *p++ = (char)('0' + ndev_unit);
  *p++ = ':';
  strcpy(p, ndev_prefix);
  p += strlen(ndev_prefix);

  for (i = 0; i < 8 && f[i] != ' '; i++)
    *p++ = f[i];
  if (f[8] != ' ') {
    *p++ = '.';
    for (i = 8; i < 11 && f[i] != ' '; i++)
      *p++ = f[i];
  }
  *p = '\0';
}

// Refill the read buffer. Only ever asks for as many bytes as status reported
// waiting: the firmware pads a longer read with stale buffer content.
static void ndev_refill(void) {
  uint16_t bw;
  uint8_t conn, err;
  int16_t got;
  unsigned int tries;

  ndev_pos = 0;
  ndev_len = 0;

  fujinet_activate();
  for (tries = 0; tries < NDEV_POLL_TRIES; tries++) {
    network_status(ndev_spec, &bw, &conn, &err);
    if (bw) {
      if (bw > sizeof(ndev_buf))
        bw = sizeof(ndev_buf);
      got = network_read(ndev_spec, ndev_buf, bw);
      if (got > 0)
        ndev_len = (uint16_t)got;
      break;
    }
    // A closed connection can still have buffered bytes, so only trust these
    // once status reports nothing waiting.
    if (err == NETWORK_ERROR_END_OF_FILE || !conn)
      break;
  }
  fujinet_deactivate();

  if (ndev_len == 0)
    ndev_ateof = 1;
}

// Push the write buffer to the device. A failure here means the file is being
// silently truncated, so it is reported rather than swallowed.
static void ndev_flush(void) {
  uint8_t err;

  if (ndev_len == 0)
    return;
  fujinet_activate();
  err = network_write(ndev_spec, ndev_buf, ndev_len);
  fujinet_deactivate();
  ndev_len = 0;
  if (err != FN_ERR_OK)
    dev_regs.err = ERR_DEVICE_IO;
}

static void ndev_result(int value) {
  MSX_DAC = value;
  MSX_VALTYP = 2;                     // integer
}

// A = 0: OPEN. E = mode, HL = I/O channel control block.
void ndev_open(void) {
  unsigned char *chan = ndev_chan();
  unsigned char mode = dev_regs.e;
  uint8_t omode;
  uint8_t err;

  if (mode == FMODE_RANDOM) {         // stream device: sequential access only
    dev_regs.err = ERR_DEVICE_IO;
    return;
  }

  ndev_make_spec();
  omode = (mode == FMODE_INPUT)  ? OPEN_MODE_READ
        : (mode == FMODE_APPEND) ? OPEN_MODE_APPEND
                                 : OPEN_MODE_WRITE;

  fujinet_activate();
  err = network_open(ndev_spec, omode, OPEN_TRANS_NONE);
  fujinet_deactivate();
  if (err != FN_ERR_OK) {
    dev_regs.err = ERR_DEVICE_IO;     // dispatcher raises it once we return
    return;                           // leave PTRFIL alone: the file is NOT open
  }

  ndev_mode     = mode;
  ndev_len      = 0;
  ndev_pos      = 0;
  ndev_ateof    = 0;
  ndev_backchar = 0;
  ndev_ascii    = 0;
  ndev_lastcr   = 0;

  chan[FL_MOD] = mode;
  chan[FL_LSA] = 0;                   // no backup character pending
  chan[FL_BPS] = 0;

  // LOAD reads FL_FLG straight out of the block to decide whether it is
  // loading a tokenized program (bit 7) or ASCII text, and nothing else
  // initialises it: OPNFIL leaves it alone, and CLSFIL skips clearing it for
  // channel 0 while a program load is in progress. So a stale bit 7 from an
  // earlier SAVE would send LOAD down the binary path with a text file. Peek
  // the first byte and set it properly.
  chan[FL_FLG] = 0;
  if (ndev_mode == FMODE_INPUT) {
    ndev_refill();
    if (ndev_len && ndev_buf[0] == 0xFF)
      chan[FL_FLG] = 0x80;            // 0FFH lead byte = tokenized BASIC
    else
      ndev_ascii = 1;                 // text: normalise line endings on the way in
  }

  MSX_PTRFIL = chan;                  // this is what tells BASIC the file is open
}

// A = 2: CLOSE. BASIC clears PTRFIL itself, so this only has to flush.
void ndev_close(void) {
  if (ndev_mode != FMODE_INPUT) {
    // An ASCII BASIC file has to end with a Ctrl-Z; BASIC does not append it
    // for a device, so we do, the same way the MSX serial ROM's COM: close does.
    if (ndev_len >= sizeof(ndev_buf))
      ndev_flush();
    ndev_buf[ndev_len++] = 0x1A;
    ndev_flush();
  }

  fujinet_activate();
  network_close(ndev_spec);
  fujinet_deactivate();

  ndev_mode     = 0;
  ndev_len      = 0;
  ndev_pos      = 0;
  ndev_ateof    = 0;
  ndev_backchar = 0;
  ndev_ascii    = 0;
  ndev_lastcr   = 0;
}

// A = 4: random access. Not meaningful for a stream.
void ndev_random(void) {
  dev_regs.err = ERR_DEVICE_IO;
}

// A = 6: sequential output - C holds the byte (PRINT#, SAVE).
void ndev_output(void) {
  ndev_buf[ndev_len++] = dev_regs.c;
  if (ndev_len >= sizeof(ndev_buf))
    ndev_flush();
  dev_regs.carry = 0;
}

// A = 8: sequential input - return the byte in A (INPUT#, LOAD, RUN).
// Carry set means end of file; BASIC turns that into "Input past end".
void ndev_input(void) {
  if (ndev_backchar) {                // a backed-up character is pending
    if (ndev_backchar == 0x1A) {
      ndev_ateof = 1;
      dev_regs.carry = 1;
      return;
    }
    dev_regs.a = ndev_backchar;
    ndev_backchar = 0;
    dev_regs.carry = 0;
    return;
  }

  for (;;) {
    unsigned char c;

    if (ndev_pos >= ndev_len) {
      if (ndev_ateof) {
        dev_regs.carry = 1;
        return;
      }
      ndev_refill();
      if (ndev_len == 0) {
        dev_regs.carry = 1;
        return;
      }
    }

    // Ctrl-Z ends a text stream: report end of file rather than handing the
    // marker over. DSKCHI, which reads program lines during LOAD, has no test
    // for 1AH at all - it stops only on CR or on carry - so a device that
    // returns the marker as data produces a spurious trailing line and breaks
    // the load. This is the same rule EOF() above uses.
    if (ndev_buf[ndev_pos] == 0x1A) {
      ndev_ateof = 1;
      dev_regs.carry = 1;
      return;
    }

    c = ndev_buf[ndev_pos++];

    // BASIC ends a program line at CR and nothing else, so a text file using
    // bare LFs (anything written on a modern machine) never terminates a line
    // at all: BASIC keeps appending to BUF, a 258-byte buffer, and a file any
    // longer than that runs off the end of it into TXTTAB and VARTAB just
    // above - which looks like a stray "Overflow" and a program that has
    // vanished. So fold every convention down to the CR that BASIC wants:
    // a lone LF becomes CR, and the LF of a CRLF pair is dropped.
    if (ndev_ascii) {
      if (c == 0x0A) {
        if (ndev_lastcr) {            // second half of CRLF, already reported
          ndev_lastcr = 0;
          continue;
        }
        c = 0x0D;
      }
      ndev_lastcr = (c == 0x0D);
    }

    dev_regs.a = c;
    dev_regs.carry = 0;
    return;
  }
}

// A = 10/12/16: LOC(), LOF(), FPOS(). A network stream has no length or
// seekable position to report, so these answer 0 rather than guess.
void ndev_loc(void) {
  ndev_result(0);
}

void ndev_lof(void) {
  ndev_result(0);
}

void ndev_fpos(void) {
  ndev_result(0);
}

// A = 14: EOF(). -1 at end of file, 0 otherwise, returned via DAC.
//
// For a stream device MSX defines end-of-file as "the next byte is the Ctrl-Z
// marker", not "the stream has run dry" - the serial ROM's COM: EOF reads the
// next character, compares it with 1AH and pushes it back. BASIC's LOAD relies
// on this: if EOF only became true once the stream was exhausted, LOAD would
// read straight past the terminator and overrun its line buffer. We own the
// read buffer, so we can peek without the read-and-push-back dance.
void ndev_eof(void) {
  if (ndev_mode != FMODE_INPUT) {     // nothing to read ahead on an output file,
    ndev_result(0);                   // and refilling would clobber the write buffer
    return;
  }

  if (ndev_backchar) {                // a pushed-back byte is what comes next
    ndev_result(ndev_backchar == 0x1A ? -1 : 0);
    return;
  }

  if (ndev_pos >= ndev_len && !ndev_ateof)
    ndev_refill();

  if (ndev_pos >= ndev_len) {         // genuinely nothing left, marker or not
    ndev_result(-1);
    return;
  }

  ndev_result(ndev_buf[ndev_pos] == 0x1A ? -1 : 0);
}

// A = 18: backup character - C is pushed back for the next sequential input.
void ndev_backup(void) {
  ndev_backchar = dev_regs.c;
}

// CALL NCD(path$)
//   Sets the path that "N:" filenames hang off, e.g.
//     CALL NCD("TNFS://HOST/BAS/")
//     SAVE "N:PROG.BAS"      -> N1:TNFS://HOST/BAS/PROG.BAS
//   Needed because BASIC hands a device only an 8.3 filename, never the
//   whole string the user typed, so a full URL cannot arrive that way.
void basic_ncd(void) {
  char *p;

  cmd_expect('(');
  p = cmd_get_string();
  cmd_expect(')');

  if (strlen(p) >= sizeof(ndev_prefix))
    cmd_error(ERR_ILLEGAL_FUNCTION);
  strcpy(ndev_prefix, p);
}
