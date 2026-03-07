# T-1.1.4N-R1 OLED Phase Report

## 1. Hardware Back-Side Mode Check (manual)
> PM note: continue this round as I2C module path; keep this section as checklist evidence.

### Module A
- Marking:
- R1/R2 (or equivalent solder option):
- Current solder state:
- Mode conclusion: `SPI default / I2C-3C / I2C-3D / UNKNOWN`

### Module B
- Marking:
- R1/R2 (or equivalent solder option):
- Current solder state:
- Mode conclusion: `SPI default / I2C-3C / I2C-3D / UNKNOWN`

## 2. Address Audit Record
- Configured `addr7`:
- Configured `addr_hal`:
- Live-watch `g_oled_addr7_dbg`:
- Live-watch `g_oled_addr_hal_dbg`:
- Pass criterion: no mixed usage (`0x3C` vs `0x78`) in HAL calls.

## 3. Phase Log Record (first-page focus)
| Time | Phase ID | Page | Chunk | HAL Status | cmd_first | data_first8 |
|---|---:|---:|---:|---|---|---|
| PENDING |  |  |  |  |  |  |

## 4. Remap Matrix Result (P3/P4 only)
| remap idx | SEG/COM | P3 result | P4 result | Notes |
|---:|---|---|---|---|
| 0 | A1 + C8 | PENDING | PENDING | |
| 1 | A0 + C8 | PENDING | PENDING | |
| 2 | A1 + C0 | PENDING | PENDING | |
| 3 | A0 + C0 | PENDING | PENDING | |

## 5. Cold/Reset Compare
### Cold power-on (>=2s power off, no RST)
- Probe:
- A5/A4:
- P1..P5:
- Failure point (if any): stage/page/chunk/status

### Hold-RST release path
- Probe:
- A5/A4:
- P1..P5:
- Failure point (if any): stage/page/chunk/status

## 6. Final Conclusion (choose one only)
- [ ] Hardware mode mismatch (SPI/I2C address strap issue)
- [ ] HAL address parameter mismatch fixed
- [ ] Display remap/orientation mismatch fixed by matrix
- [ ] Cross-module failure persists, replace with known-fixed I2C OLED module

## 7. Return-to T-1.1.5 Gate
- [ ] P3 (top white / bottom black) stable correct
- [ ] P4 (left white / right black) stable correct
