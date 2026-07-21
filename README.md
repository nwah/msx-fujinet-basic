# FujiNet BASIC for MSX

MSX-BASIC extension commands for the [FujiNet](https://fujinet.online) device, compiled as a ROM cartridge with [z88dk](https://z88dk.org).

## Building

```
make
```

Requires z88dk and the `fujinet-lib` prebuilt library under `_cache/`. The build produces a
padded 16K `fujinet-basic.rom`.

### Memory

The cartridge keeps its work buffers in RAM at `0xD700`, and lowers `HIMEM` to that address at
boot so BASIC will not allocate over them. This costs roughly 6.75K of the memory BASIC would
otherwise report as free.

## Conventions

- All commands are invoked with `CALL <NAME>(args)`.
- Commands with no arguments omit the parentheses entirely: `CALL NINIT`.
- `%` suffix denotes a 16-bit integer variable (e.g. `S%`, `BW%`).
- `$` suffix denotes a string variable (e.g. `A$`, `DEV$`).
- Output arguments are always last, matching the `S%` / `result$` pattern.
- **devicespec** format: `N1:PROTO://hostname/path` (e.g. `"N1:HTTPS://fujinet.online/"`).
- Slot indices are 0-based (0–7).

---

## General

| Command | Description |
|---|---|
| `CALL FUJINET` | Print version banner (`FujiNet BASIC vX.Y.Z`). Also printed automatically once at startup, right after BASIC's "Bytes free" line and before the `Ok` prompt - the same spot real disk BASIC ROMs print their banner. |
| `CALL FNCONFIG` | Print adapter config: SSID, IP, MAC, firmware version, etc. |
| `CALL FNGETDEVICE(slot, name$)` | Read the mounted filename for device `slot` into `name$`. |

---

## Network — Connection

| Command | Description |
|---|---|
| `CALL NINIT` | Initialise the network device. |
| `CALL NOPEN(dev$, mode, trans)` | Open a connection. See **Open modes** and **Translation modes** below. |
| `CALL NCLOSE(dev$)` | Close the connection on `dev$`. |
| `CALL NSTATUS(dev$, bw%, conn%, err%)` | Query connection status. `bw%` = bytes waiting, `conn%` = connected flag, `err%` = error byte. |
| `CALL NACCEPT(dev$)` | Accept an incoming connection on `dev$`. |

### Open modes

| Value | Constant | Meaning |
|---|---|---|
| 4 | `OPEN_MODE_READ` / `OPEN_MODE_HTTP_GET` | Read / HTTP GET |
| 8 | `OPEN_MODE_WRITE` / `OPEN_MODE_HTTP_PUT` | Write / HTTP PUT |
| 12 | `OPEN_MODE_RW` / `OPEN_MODE_HTTP_GET_H` | Read+Write / HTTP GET with headers |
| 13 | `OPEN_MODE_HTTP_POST` | HTTP POST |
| 14 | `OPEN_MODE_HTTP_PUT_H` | HTTP PUT with headers |
| 5 | `OPEN_MODE_HTTP_DELETE` | HTTP DELETE |
| 9 | `OPEN_MODE_HTTP_DELETE_H` | HTTP DELETE with headers |

### Translation modes

| Value | Meaning |
|---|---|
| 0 | None |
| 1 | CR |
| 2 | LF |
| 3 | CR+LF |
| 4 | PETSCII |

---

## Network — Data

| Command | Description |
|---|---|
| `CALL NREAD(dev$, len, result$, S%)` | Blocking read of up to `len` bytes into `result$`. `S%` receives the actual byte count. |
| `CALL NREADNB(dev$, len, result$, S%)` | Non-blocking read. Returns immediately with however many bytes are available. |
| `CALL NWRITE(dev$, data$)` | Write `data$` to the open connection. |

---

## Network — JSON

| Command | Description |
|---|---|
| `CALL NJSONPARSE(dev$)` | Parse the currently open connection as JSON. |
| `CALL NJSONQUERY(dev$, query$, result$, S%)` | Query a JSON path (e.g. `"/items/0/name"`). `result$` receives the value, `S%` the byte count. |

---

## Network — HTTP

| Command | Description |
|---|---|
| `CALL NHTTPPOST(dev$, data$)` | Send `data$` as an HTTP POST body. |
| `CALL NHTTPPUT(dev$, data$)` | Send `data$` as an HTTP PUT body. |
| `CALL NHTTPDEL(dev$, trans)` | Open an HTTP DELETE request (read/close afterwards). |
| `CALL NHTTPSTARTHDR(dev$)` | Enter header-writing mode. |
| `CALL NHTTPADDHDR(dev$, header$)` | Add one header, e.g. `"Accept: application/json"`. |
| `CALL NHTTPENDHDR(dev$)` | Leave header-writing mode, return to body mode. |
| `CALL NHTTPMODE(dev$, mode)` | Set channel mode directly (see **HTTP channel modes** below). |

### HTTP channel modes

| Value | Meaning |
|---|---|
| 0 | Body |
| 1 | Collect headers |
| 2 | Get headers |
| 3 | Set headers |
| 4 | POST set data |

---

## Files — the `N:` device

FujiNet registers itself as an MSX-BASIC network device, so the standard file
statements work against network paths directly:

`LOAD` `SAVE` `RUN` `MERGE` `OPEN` `CLOSE` `PRINT#` `INPUT#` `LINE INPUT#` `INPUT$` `EOF`

Sequential access only. `GET#`/`PUT#` raise an error, and `LOC`/`LOF`/`FPOS` return 0 —
a network stream has no length or seekable position to report.

Use a numbered unit — `N1:` through `N8:` — where a filename would normally go; `N1:`
is unit 1.

> **Use `N1:`, not bare `N:`, when a disk drive is present.** Under Disk BASIC the
> filename parser treats a single letter as a drive letter, so `N:` is read as
> (non-existent) drive `N` and fails with `Bad drive name` before FujiNet ever sees
> it — the cartridge's device hook is only consulted for names longer than one
> character. Bare `N:` works only on a machine with no disk ROM (plain cassette
> BASIC). `N1:`–`N8:` work in both.

Because MSX-BASIC only ever hands a device an 8.3 filename — never the full string
you typed — the scheme, host and directory come from `CALL NCD` instead of the
filename itself.

| Command | Description |
|---|---|
| `CALL NCD(path$)` | Set the path that `N:` filenames hang off. Max 191 characters. |

### Example

```basic
CALL NCD("TNFS://myserver.local/BAS/")
SAVE "N1:HELLO.BAS"
NEW
LOAD "N1:HELLO.BAS"
RUN
```

`SAVE "N1:HELLO.BAS"` above resolves to `N1:TNFS://myserver.local/BAS/HELLO.BAS`.

Files are read and written as ASCII, terminated with Ctrl-Z (`&H1A`) the same way
MSX disk BASIC writes them, so programs saved here load back on any MSX.

---

## Printer

Printer output is redirected to the FujiNet printer device, so the standard
MSX-BASIC printer statements print through FujiNet instead of the Centronics port:

`LPRINT` `LPRINT USING` `LLIST` and `PRINT#` to a file opened on `LPT:`

```basic
LPRINT "HELLO FROM MSX"
LLIST

OPEN "LPT:" FOR OUTPUT AS #1
PRINT #1,"HELLO FROM MSX"
CLOSE #1
```

Whatever FujiNet's printer device is configured to emulate (PDF, HTML, an Epson,
a plotter) is what the output turns into — the MSX just hands over the bytes.

No setup is needed and nothing has to be enabled: the redirection is installed at
boot. Because it replaces the BIOS printer routines outright, a real printer on
the parallel port is not reachable while the cartridge is installed.

Characters are batched and sent a chunk at a time rather than one per statement.
A chunk is sent at the end of each line, on a form feed, and whenever 128
characters have piled up, so ordinary `LPRINT` and `LLIST` output goes out as it
is produced. Output deliberately held back with a trailing `;` has no line end to
trigger on and stays buffered until the next line completes.

| Command | Description |
|---|---|
| `CALL LPTFLUSH` | Send buffered printer output to FujiNet now. Only needed after printing that never ended a line, e.g. `LPRINT "X";`. |

> **Note:** if no FujiNet is attached, a printer statement will block while the
> device is waiting to be talked to, the same as any other FujiNet command here.

---

## WiFi

| Command | Description |
|---|---|
| `CALL FWIFIENABLED(S%)` | `S%` = 1 if WiFi is active, 0 otherwise. |
| `CALL FWIFISTATUS(S%)` | `S%` = WiFi status: 1=no SSID, 3=connected, 4=failed, 5=lost. |
| `CALL FWIFISCAN(S%)` | Scan for networks. `S%` = number found. |
| `CALL FWIFISCANRESULT(n, ssid$, rssi%)` | Get scan result `n`. `ssid$` = network name, `rssi%` = signal strength (dBm, negative). |
| `CALL FGETWIFISSID(ssid$)` | Read the currently connected SSID into `ssid$`. |
| `CALL FSETWIFISSID(ssid$, password$)` | Set WiFi credentials and connect. |

---

## Host Slots

FujiNet supports 8 host slots (0–7), each holding a URL (e.g. a TNFS server address). The `FLOAD`/`FSAVE` commands sync the in-memory cache with the device; `FGET`/`FSET` operate only on the cache.

| Command | Description |
|---|---|
| `CALL FLOADHOSTSLOTS` | Load all 8 host slots from FujiNet into the local cache. |
| `CALL FSAVEHOSTSLOTS` | Save the local cache of all 8 host slots back to FujiNet. |
| `CALL FGETHOSTSLOT(slot, name$)` | Read host slot URL from cache into `name$`. |
| `CALL FSETHOSTSLOT(slot, name$)` | Write `name$` into the cached host slot. |
| `CALL FMOUNTHOST(hs)` | Mount host slot `hs`. |
| `CALL FGETHOSTPREFIX(hs, prefix$)` | Get the directory prefix for host slot `hs`. |
| `CALL FSETHOSTPREFIX(hs, prefix$)` | Set the directory prefix for host slot `hs`. |

---

## Device Slots

FujiNet supports 8 device slots (0–7). Each slot has a host slot index, a mode, and a filename. `FLOAD`/`FSAVE` sync with the device; all other slot commands operate on the local cache.

| Command | Description |
|---|---|
| `CALL FLOADDEVSLOTS` | Load all 8 device slots from FujiNet into the local cache. |
| `CALL FSAVEDEVSLOTS` | Save the local cache of all 8 device slots back to FujiNet. |
| `CALL FGETDEVSLOTHOST(slot, S%)` | `S%` = host slot index for device slot `slot`. |
| `CALL FGETDEVSLOTMODE(slot, S%)` | `S%` = mode byte for device slot `slot`. |
| `CALL FGETDEVSLOTFILE(slot, file$)` | `file$` = filename for device slot `slot`. |
| `CALL FSETDEVSLOTHOST(slot, host)` | Set host slot index in cache. |
| `CALL FSETDEVSLOTMODE(slot, mode)` | Set mode byte in cache. |
| `CALL FSETDEVSLOTFILE(slot, file$)` | Set filename in cache (max 35 characters). |
| `CALL FMOUNT(ds, mode)` | Mount device slot `ds` with `mode` (4=read, 8=read+write). |
| `CALL FUNMOUNT(ds)` | Unmount device slot `ds`. |
| `CALL FMOUNTALL` | Mount all configured device slots. |
| `CALL FENABLEDEV(d)` | Enable device `d`. |
| `CALL FDISABLEDEV(d)` | Disable device `d`. |
| `CALL FSETFILE(ds, hs, mode, file$)` | Set the full path for device slot `ds` on host `hs` with `mode`. |

---

## Directory

Operates on a host slot previously mounted with `FMOUNTHOST`. Read entries one at a time with `FREADDIR` until an empty string is returned.

| Command | Description |
|---|---|
| `CALL FOPENDIR(hs, path$)` | Open directory `path$` on host slot `hs`. |
| `CALL FOPENDIREX(hs, path$, filter$)` | Open directory with a filename filter (e.g. `"*.DSK"`). |
| `CALL FCLOSEDIR` | Close the currently open directory. |
| `CALL FREADDIR(result$)` | Read the next directory entry into `result$`. Empty string = end of listing. |
| `CALL FSETDIRPOS(pos)` | Seek to position `pos` within the current directory listing. |

---

## Boot

| Command | Description |
|---|---|
| `CALL FSETBOOTCFG(toggle)` | Enable (1) or disable (0) FujiNet config-boot mode. |
| `CALL FSETBOOTMODE(mode)` | Set FujiNet boot mode (e.g. lobby). |

---

## Hash

Supports MD5, SHA1, SHA256, and SHA512. Two workflows are available:

- **Single-shot**: `FHASHDATA` hashes a string in one call.
- **Accumulated**: call `FHASHCLEAR`, one or more `FHASHADD`, then `FHASHCALC`.

Use `hex=1` when capturing results in a BASIC string variable; binary output (`hex=0`) may be truncated if the hash bytes contain embedded nulls.

### Hash type values

| Value | Algorithm | Hex output length | Binary output length |
|---|---|---|---|
| 0 | MD5 | 32 | 16 |
| 1 | SHA1 | 40 | 20 |
| 2 | SHA256 | 64 | 32 |
| 3 | SHA512 | 128 | 64 |

| Command | Description |
|---|---|
| `CALL FHASHCLEAR` | Clear any previously accumulated hash data on FujiNet. |
| `CALL FHASHADD(data$)` | Add `data$` to the accumulated hash input. |
| `CALL FHASHCALC(type, hex, discard, result$)` | Compute hash of accumulated data. `type` = algorithm (0–3), `hex` = 1 for hex string / 0 for binary, `discard` = 1 to free data afterwards. Result written to `result$`. |
| `CALL FHASHDATA(type, data$, hex, result$)` | Single-shot hash of `data$`. Arguments as above. |

### Example — SHA256 of a string

```basic
CALL FHASHDATA(2, "Hello, world!", 1, H$)
PRINT H$
```

---

## App Keys

App Keys may be used by program to store user preferences or other small amounts of data on the FujiNet.

| Size | Value | Notes |
| ---  | ---   | ---   |
| 2 bytes | creator ID | see below |
| 1 byte | app ID | Creator-specified value (0-255) |
| 1 byte | key ID | Creator-specified value (0-255) |

| Command | Description |
|---|---|
| `CALL FSETAPPKEY(creator_id, app_id)` | Set credentials for subsequent app key operations. Must be called first. |
| `CALL FWRITEAPPKEY(key_id, data$)` | Write data$ to app key key_id (max 64 bytes). |
| `CALL FREADAPPKEY(key_id, result$, S%)` | Read app key key_id into result$; S% receives the byte count. |

App keys are stored as files in the `/FujiNet` directory on the SD card. **Note that only SD storage is currently supported for app key storage, and app key reads/writes will fail if no SD card is installed.**  The file name is constructed using the hexadecimal values for creator, app, and key.  e.g. `/FujiNet/B0C1010A.key`

### Creator IDs

This is an unsigned 16-bit value, which should allow for more than enough creators. Values from 0-255 (0x00-0xFF) are reserved for internal use by FujiNet - please don't use any value in this range. See FujiNet wiki for [list of Creator IDs](https://github.com/FujiNetWIFI/fujinet-firmware/wiki/SIO-Command-%24DC-Open-App-Key#known-creator-ids-and-appids).

---
