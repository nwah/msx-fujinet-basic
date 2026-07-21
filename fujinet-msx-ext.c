/**
 * @file fujinet-msx-ext.c
 * @brief MSX implementations of bus functions absent from the prebuilt library.
 *
 * fuji_bus_appkey_read/write and fuji_base64_* are declared in the FujiNet
 * headers but not yet compiled into fujinet.msx.lib.  Every function here is
 * implemented with the same FUJICALL_* macros that the rest of the library
 * uses; they all bottom out in fuji_bus_call which IS in the prebuilt object.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "fujinet-fuji.h"
#include "fujinet-commands.h"
#include "fujinet-bus.h"
#include "fujinet-const.h"


typedef struct {
  uint16_t length;
  uint8_t data[MAX_APPKEY_LEN];
} FNAppKeyString;

// ---------------------------------------------------------------------------
// App Key bus transport
// ---------------------------------------------------------------------------
//
FNAppKeyString appkey_buf;

bool fuji_bus_appkey_read(void *string, uint16_t *length)
{
  // Caller may not have room for length header so use our own buffer to read
   if (!FUJICALL_RV(FUJICMD_READ_APPKEY, &appkey_buf, sizeof(appkey_buf)))
     return false;
   *length = appkey_buf.length;
   memmove(string, appkey_buf.data, appkey_buf.length);
   return true;
}

bool fuji_bus_appkey_write(void *string, uint16_t length)
{
  return FUJICALL_D(FUJICMD_WRITE_APPKEY, string, length);
}

// Base64 encode/decode transport removed: the FB64* BASIC commands are
// currently NOOP stubs (see the base64 section in basic.c), so these
// implementations are unused. Restore from git history when re-enabling.
