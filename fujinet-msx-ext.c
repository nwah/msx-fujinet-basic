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

// ---------------------------------------------------------------------------
// Base64 encode
// ---------------------------------------------------------------------------

bool fuji_base64_encode_input(char *s, uint16_t len)
{
  return FUJICALL_D(FUJICMD_BASE64_ENCODE_INPUT, s, len);
}

bool fuji_base64_encode_compute(void)
{
  return FUJICALL(FUJICMD_BASE64_ENCODE_COMPUTE);
}

bool fuji_base64_encode_length(unsigned long *len)
{
  uint16_t l = 0;
  bool ok = FUJICALL_RV(FUJICMD_BASE64_ENCODE_LENGTH, &l, sizeof(l));
  if (ok) *len = (unsigned long)l;
  return ok;
}

bool fuji_base64_encode_output(char *s, uint16_t len)
{
  return FUJICALL_RV(FUJICMD_BASE64_ENCODE_OUTPUT, s, len);
}

// ---------------------------------------------------------------------------
// Base64 decode
// ---------------------------------------------------------------------------

bool fuji_base64_decode_input(char *s, uint16_t len)
{
  return FUJICALL_D(FUJICMD_BASE64_DECODE_INPUT, s, len);
}

bool fuji_base64_decode_compute(void)
{
  return FUJICALL(FUJICMD_BASE64_DECODE_COMPUTE);
}

bool fuji_base64_decode_length(unsigned long *len)
{
  uint16_t l = 0;
  bool ok = FUJICALL_RV(FUJICMD_BASE64_DECODE_LENGTH, &l, sizeof(l));
  if (ok) *len = (unsigned long)l;
  return ok;
}

bool fuji_base64_decode_output(char *s, uint16_t len)
{
  return FUJICALL_RV(FUJICMD_BASE64_DECODE_OUTPUT, s, len);
}
