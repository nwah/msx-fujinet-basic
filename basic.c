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



// ============================================================
// Static globals shared by the commands below
// ============================================================
static NetConfig  net_config;
static HostSlot   host_slots[8];
static DeviceSlot device_slots[8];
static SSIDInfo   ssid_info;
// binary output sizes for MD5, SHA1, SHA256, SHA512
static const uint8_t hash_bin_sizes[4] = {16, 20, 32, 64};

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
  fuji_hash_add((uint8_t*)strbuf, len);
  fujinet_deactivate();
}

// CALL FHASHCALC(type, hex, discard, result$)
void basic_fhashcalc(void) {
  cmd_expect('(');
  int type = cmd_get_int();
  cmd_expect(',');
  int as_hex = cmd_get_int();
  cmd_expect(',');
  int discard = cmd_get_int();
  cmd_expect(',');
  cmd_get_var();
  cmd_expect(')');

  memset(buf2, 0, sizeof(buf2));
  fujinet_activate();
  fuji_hash_calculate((hash_alg_t)type, (bool)as_hex, (bool)discard, (uint8_t*)buf2);
  fujinet_deactivate();

  if (!as_hex && type >= 0 && type < 4)
    buf2[hash_bin_sizes[type]] = '\0';

  cmd_set_string(buf2);
}

// CALL FHASHDATA(type, data$, hex, result$)
void basic_fhashdata(void) {
  cmd_expect('(');
  int type = cmd_get_int();
  cmd_expect(',');
  cmd_get_string();                          // data$ -> strbuf
  uint16_t len = (uint16_t)strlen(strbuf);  // measure before strbuf is reused
  cmd_expect(',');
  int as_hex = cmd_get_int();
  cmd_expect(',');
  cmd_get_var();
  cmd_expect(')');

  memset(buf2, 0, sizeof(buf2));
  fujinet_activate();
  fuji_hash_data((hash_alg_t)type, (uint8_t*)strbuf, len, (bool)as_hex, (uint8_t*)buf2);
  fujinet_deactivate();

  if (!as_hex && type >= 0 && type < 4)
    buf2[hash_bin_sizes[type]] = '\0';

  cmd_set_string(buf2);
}
