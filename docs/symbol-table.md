# The embedded symbol table in the M10 system image

**Status: verified on M10 3.22.23.38, M10 4.26.36.10 and M10-P 4.26.**
This is the single most useful discovery for working on this firmware, because it
turns anonymous code into named code. Before it, every function had to be found by
following call graphs from string references. After it, about 250 functions have
real names.

---

## 1. What it is, and why it is easy to miss

The system image carries small tables of `(pointer, name)` pairs left over from the
build. They are not a full symbol table — they cover debug entry points, utility
functions, the capture path and hardware driver roots.

Entry layout:

```
u32  ptr      = 0x42000000 + <file offset of the target> + 1
                the low bit is the ARM/Thumb interworking bit; mask it off
char name[]   NUL terminated ASCII, zero padded to 4-byte alignment
```

Two traps make naive scanning fail:

1. **The high byte of every pointer is `0x42`, which is ASCII `B`.**
   If you scan for "printable strings", you start one byte late, land inside the
   pointer, and never see the table. This is why string-based surveys of this image
   previously reported nothing.
2. **You cannot test `ptr == base + offset`.** The `+1` Thumb bit must be masked.
   Testing equality exactly rejects every entry.

## 2. How to extract them

An entry is accepted when either

- it **chains**: the next entry starts at the 4-byte-aligned end of this name, or
- its pointer targets a **plausible prologue**.

The prologue test must respect endianness. Thumb `PUSH {..., LR}` is the halfword
`0xB5xx`; on disk little-endian that means the **second** byte is `0xB5`. Checking
the first byte finds nothing, because that byte is only the low part of the register
mask. ARM `PUSH` (`0xE92Dxxxx`) appears on disk as the register list followed by
`2d e9`.

A working implementation is in [`tools/m10sym/extract.py`](../tools/m10sym/extract.py):

```sh
python3 tools/m10sym/extract.py system.bin --save syms.txt
python3 tools/m10sym/extract.py system.bin --filter touch
```

Resulting counts: **246** symbols (M10 3.22), **245** (M10 4.26), **243** (M10-P 4.26).

> Note when extending the extractor: relaxing the chain rule to accept single
> entries whose pointer merely lands inside the image produces hundreds of false
> positives (`BAAA`, `SD0`, `.DNG`, MP4 atom names). Those are plain string/pointer
> constants, not symbols. Requiring a prologue, or requiring a chain of two or
> more, is what makes the output clean.

## 3. What it unlocks

### 3.1 Capture pipeline — fully named on M10 3.22

| Address | Symbol |
|---|---|
| `0x421CB9B8` | `capture_still` |
| `0x421CC774` | `cap_still_prepare_capture` |
| `0x421CC40C` | `cap_still_capture` |
| `0x421CC3F4` | `cap_still_capture_black` |
| `0x421CCAF6` | `cap_still_readout_sensor` |
| `0x421CC964` | `cap_still_process_image` |
| `0x421CCC14` | `cap_still_store_image` |
| `0x421CCC88` / `0x421CCCB0` | `cap_still_wait_for_ccpu` / `_for_img` |
| `0x421C146C` | `cap_liveview_init` |

This is the whole stills path end to end. Any work that has to hook "just before the
image is processed" or "just before it is stored" now has an address.

### 3.2 Settings persistence

| Address | Symbol |
|---|---|
| `0x4213D508` | `hsk_save_settings` |
| `0x4213D4F4` | `hsk_reset_settings` |
| `0x4209F2B8` | `write_file` |
| `0x4209F3D4` | `write_memory_to_file` |
| `0x4205FA38` | `read_file` |

### 3.3 Firmware update / power

| Address | Symbol |
|---|---|
| `0x42133AAC` | `hsk_UpdateFirmwareFromSDCard` |
| `0x42133BFC` | `hsk_UpdateLensFirmwareFromSDCard` |
| `0x42131DD4` | `hsk_FlashFileFromSDCardForDebug` |
| `0x4217EE3C` (4.26) / `0x42179110` (3.22) | `power` |
| `0x421D4FB0` | `run` |
| `0x421D518C` | `sleep` |
| `0x421D4DF2` | `prerun` |
| `0x421D4E04` | `presleep` |
| `0x421D4E70` | `restore_firmware` |

### 3.4 State machine and debugging

`sys_SendEventByName` (`0x421ACA50`), `sys_still_SendEventByName` (`0x421CBA74`),
`sys_GetActualCameraState` (`0x421AC514`), `sys_print_state` (`0x421CADBC`),
plus the whole `sys_wifi_module_*` family and `sys_evf_status_*` family.

### 3.5 Touch-related entries

```
0x421C6714  hollywood_ui_menu_debug_touchscreen_test
0x421C67FC  hollywood_ui_menu_keypad_untouched
0x421CA220  play_single_3x4_touch_init
0x421CA24C  play_single_4x5_touch_init
0x421CA2E0  play_single_fit_touch_init
```

Note that several names alias one address (`0x421C6714` is also
`hollywood_ui_menu_debug_gps_status` and `hollywood_ui_menu_time_hhmmss_hours`,
`0x421C67FC` is also `hollywood_ui_menu_key_W` and `_key_backspace`). That is a
property of how these tables were generated — the same dispatcher serves several
menu entries — not an extraction error. Treat a shared address as one function.

## 4. Cross-version diffing

Because the name sets are nearly identical across models, the tables give a cheap
way to compute displacement between firmware versions: look up the same name in two
images and subtract. M10 3.22 and M10 4.26 differ by three data-symbol entries
(`tYMB`, `h_MB`, `DdMB` in the `dref`/`stbl`/`stsd` tables); M10 and M10-P name sets
match.

## 5. Limits

- Not a full symbol table. Roughly 250 names for a ~9.8 MB image.
- No types, no sizes, no source files.
- Names describe intent, not behaviour. Every claim still has to be confirmed by
  reading the code — the touch entries above are a good example, where the name
  says "touch_init" but the function's first action is to bail out if a global
  pressed-state flag is zero.
