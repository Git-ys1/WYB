# T-1.1.4N-R1 OLED Driver Report

## 1. Scope Freeze
- OLED ?????????/??/???
- Backend: `HW I2C2` only, no Soft-I2C/recover/dirty flush.
- Profile: fixed `SSD1315`.

## 2. HAL Address Audit
- Unified macro:
  - `OLED_ADDR_7BIT = 0x3C` (change to `0x3D` only if hardware check confirms)
  - `OLED_ADDR_HAL = OLED_ADDR_7BIT << 1`
- HAL API usage:
  - `HAL_I2C_IsDeviceReady(..., OLED_ADDR_HAL, ...)`
  - `HAL_I2C_Master_Transmit(..., OLED_ADDR_HAL, ...)`
- Live-watch mirrors in `main.c`:
  - `g_oled_addr7_dbg`
  - `g_oled_addr_hal_dbg`

## 3. Init Profiles (A/B)
### Init-A (`OLED_INIT_PROFILE_A=1`)
`AE D5 80 A8 3F D3 00 40 8D 14 20 02 A1 C8 DA 12 81 CF D9 F1 DB 40 A4 A6 2E AF`

### Init-B (`OLED_INIT_PROFILE_A=0`)
`AE D3 00 40 20 02 A1 C8 DA 12 A6 A4 AF`

Common timing:
- power-on guard before first probe: `250ms`
- after `AF`: `100ms`

## 4. Flush Method (N-R1)
- Page mode only.
- Per page command:
  1. `B0 + page`
  2. `00`
  3. `10`
- Data: 16-byte chunked write.
  - each tx: `0x40 + 16B`
  - each page: 8 chunks (`chunk 0..7`)
- Timeout:
  - probe: `25ms`
  - cmd write: `25ms`
  - data chunk: `25ms`

## 5. Phase Pattern Definition
- P1: Black
- P2: White
- P3: Top half white / bottom half black
- P4: Left half white / right half black
- P5: 1px border

PB12 phase marker:
- 1 blink -> P1
- 2 blink -> P2
- 3 blink -> P3
- 4 blink -> P4
- 5 blink -> P5

PB12 error marker:
- 1 blink -> probe fail
- 2 blink -> init fail
- 3 blink -> flush fail

## 6. Remap Matrix (P3/P4 only)
- `A1 + C8`
- `A0 + C8`
- `A1 + C0`
- `A0 + C0`

Live-watch remap index:
- `g_oled_remap_idx_dbg` (`0..3`)

## 7. Phase Log and Failure Location
From `oled_smoke_get_phase_log()`:
- `phase_id`
- `page`
- `chunk`
- `hal_status`
- `cmd_first[2]`
- `data_first8[8]`

From `oled_smoke_diag_get()`:
- `stage` (`PROBE / INIT / FLUSH_CMD / FLUSH_DATA`)
- `page`
- `chunk`
- `hal_status`

## 8. First Packet Capture (N-R1)
- First command packet: `00 xx` (cmd byte depends on current phase/init)
- First data payload 8 bytes captured in phase log field `data_first8`

## 9. Observation Table (fill on bench)
| Item | Result | Notes |
|---|---|---|
| Addr audit (7-bit/HAL) | PENDING | |
| Init-A behavior | PENDING | |
| Init-B behavior | PENDING | |
| P1/P2/P5 baseline | PENDING | |
| Remap idx0 (A1+C8) P3/P4 | PENDING | |
| Remap idx1 (A0+C8) P3/P4 | PENDING | |
| Remap idx2 (A1+C0) P3/P4 | PENDING | |
| Remap idx3 (A0+C0) P3/P4 | PENDING | |

## 10. T-1.5.1A Baseline Reuse
- OLED low-level parameters remain frozen from smoke pass:
  - `SSD1315 profile`
  - `HW I2C2 @100k`
  - `page mode`
  - `cmd control byte 0x00`
  - `data control byte 0x40`
  - `16-byte chunk flush`
- Integration target for this round:
  - fixed text page first
  - then fixed ADC debug page (`RAW/MV/VDDA/STAT`)
- Old complex display path remains paused:
  - no Soft-I2C fallback
  - no recover state machine
  - no dirty flush

## 11. T-1.1.5C-R1 Character Fix
- Symptom: some uppercase letters were blank on smoke text pages (especially `V/W/X/Y/Z` class).
- Root cause: `bsp_oled_smoke.c` used a sparse switch-based glyph map; many ASCII letters had no glyph entry and fell back to space.
- Fix:
  - replaced sparse switch with full table lookup (`A-Z`, `0-9`, punctuation used by UI),
  - lowercase auto-upcase before lookup,
  - unknown chars still fall back to space.
- Rendering policy unchanged:
  - fixed `5x7` glyph,
  - fixed `6px` step (`5 columns + 1 spacing`),
  - unchanged OLED low-level path (`SSD1315/HW-I2C2/100k/page/16B chunk`).
