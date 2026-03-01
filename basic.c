// #include <stdbool.h>
#include <string.h>
#include <conio.h>
// #include <stdio.h>
#include "fujinet-fuji.h"
#include "basic.h"

AdapterConfigExtended c;

void __FASTCALL__ basic_fnconfig(void) {
  // char msg[40];

  cputs("FujiNet Config\n\n");
  // __asm
  //   ld    A,$D0
  //   out   ($A8),A
  // __endasm;

  fuji_get_adapter_config_extended(&c);
  // printf("Hello\n");

  cprintf(" %8s %s\n","SSID",c.ssid);
  cprintf(" %8s %s\n","HOSTNAME",c.hostname);
  cprintf(" %8s %u.%u.%u.%u\n","IP",c.localIP[0],c.localIP[1],c.localIP[2],c.localIP[3]);
  cprintf(" %8s %u.%u.%u.%u\n","NETMASK",c.netmask[0],c.netmask[1],c.netmask[2],c.netmask[3]);
  cprintf(" %8s %u.%u.%u.%u\n","DNS",c.dnsIP[0],c.dnsIP[1],c.dnsIP[2],c.dnsIP[3]);
  cprintf(" %8s %02x:%02x:%02x:%02x:%02x:%02x\n","MAC",c.macAddress[0],c.macAddress[1],c.macAddress[2],c.macAddress[3],c.macAddress[4],c.macAddress[5]);
  cprintf(" %8s %02x:%02x:%02x:%02x:%02x:%02x\n","BSSID",c.bssid[0],c.bssid[1],c.bssid[2],c.bssid[3],c.bssid[4],c.bssid[5]);
  cprintf(" %8s %s\n","FN VER",c.fn_version);

  // cputs("\nSSID: "); cputs(c.ssid);
  // cputs("\nHOSTNAME: "); cputs(c.hostname);
  // cputs("\nIP: "); cputs(c.localIP[0]); cputc('.'); cputs(c.localIP[1]); cputc('.'); cputs(c.localIP[2]); cputc('.'); cputs(c.localIP[3]);
  // cputs("\nNETMASK: "); cputs(c.netmask[0]); cputc('.'); cputs(c.netmask[1]); cputc('.'); cputs(c.netmask[2]); cputc('.'); cputs(c.netmask[3]);
  // cputs("\nDNS: "); cputs(c.dnsIP[0]); cputc('.'); cputs(c.dnsIP[1]); cputc('.'); cputs(c.dnsIP[2]); cputc('.'); cputs(c.dnsIP[3]);
  // cputs("\nMAC: "); cputs(c.macAddress[0]); cputc(':'); cputs(c.macAddress[1]); cputc(':'); cputs(c.macAddress[2]); cputc(':'); cputs(c.macAddress[3]); cputc(':'); cputs(c.macAddress[4]); cputc(':'); cputs(c.macAddress[5]);
  // cputs("\nBSSID: "); cputs(c.bssid[0]); cputc('.'); cputs(c.bssid[1]); cputc('.'); cputs(c.bssid[2]); cputc('.'); cputs(c.bssid[3]); cputc('.'); cputs(c.bssid[4]); cputc('.'); cputs(c.bssid[5]);
  // cputs("\nFN VER: "); cputs(c.fn_version);
}
