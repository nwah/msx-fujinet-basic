#include <stdbool.h>
#include <stdio.h>
#include <conio.h>
#include "fujinet-fuji.h"
#include "fujinet-network.h"
#include "basic.h"

AdapterConfigExtended c;
char buf[256];

void __FASTCALL__ basic_fnconfig(void) {
  cputs("FujiNet Config\n\n");

  fuji_get_adapter_config_extended(&c);

  cprintf(" %8s %s\n","SSID",c.ssid);
  cprintf(" %8s %s\n","HOSTNAME",c.hostname);
  cprintf(" %8s %u.%u.%u.%u\n","IP",c.localIP[0],c.localIP[1],c.localIP[2],c.localIP[3]);
  cprintf(" %8s %u.%u.%u.%u\n","NETMASK",c.netmask[0],c.netmask[1],c.netmask[2],c.netmask[3]);
  cprintf(" %8s %u.%u.%u.%u\n","DNS",c.dnsIP[0],c.dnsIP[1],c.dnsIP[2],c.dnsIP[3]);
  cprintf(" %8s %02x:%02x:%02x:%02x:%02x:%02x\n","MAC",c.macAddress[0],c.macAddress[1],c.macAddress[2],c.macAddress[3],c.macAddress[4],c.macAddress[5]);
  cprintf(" %8s %02x:%02x:%02x:%02x:%02x:%02x\n","BSSID",c.bssid[0],c.bssid[1],c.bssid[2],c.bssid[3],c.bssid[4],c.bssid[5]);
  cprintf(" %8s %s\n","FN VER",c.fn_version);
}

bool basic_fngetdevice(int ds, char *buffer) {
  return fuji_get_device_filename(ds, buf);
}

int basic_nopen(const char* devicespec, int mode, int trans) {
  network_open(devicespec, (uint8_t)mode, (uint8_t)trans);
  return 1;
  // return network_open(devicespec, (uint8_t)mode, (uint8_t)trans);
}
