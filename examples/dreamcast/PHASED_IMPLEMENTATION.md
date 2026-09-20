# PocketDC — Phased Implementation

This document tracks the implementation plan for **PocketDC**, a Dreamcast port of **Walnut-CGB** via **KallistiOS (KOS)**. Platform work lives in `examples/dreamcast/`. The core (`walnut_cgb.h`) includes an MIT savestate API (`gb_serialize` / `gb_deserialize`); PocketDC writes slot 0 as `.ss0` beside the ROM.

## Feasibility Summary

| Factor | Assessment |
|--------|------------|
| CPU | SH-4 @ 200 MHz is sufficient for Game Boy emulation with `gb_run_frame_dualfetch()` |
| Endianness | SH-4 is little-endian — satisfies `WALNUT_GB_IS_LITTLE_ENDIAN=1` |
| RAM | 16 MB main RAM; typical usage under 6 MB including ROM |
| Display | 160×144 native; 3×/4× integer, widescreen, or full-screen scale on 640×480 |
| Video out | Auto-detect VGA cable; force VGA or TV in settings |
| Audio | AICA @ 44.1 kHz stereo via `snd_stream` + MiniGB APU |
| Library fit | Callback-driven API maps cleanly to a KOS frontend |

## Repository Layout

```
examples/dreamcast/
├── PHASED_IMPLEMENTATION.md   # This file
├── README.md                  # Build and run instructions
├── Makefile                   # KOS build
├── dc_priv.h                  # Shared frontend state
├── walnut_dc.c                # Main, callbacks, emulation loop
├── video.c / video.h          # PVR texture upload, scale modes, overlays
├── display.c / display.h      # VGA/TV/auto video output
├── audio.c / audio.h          # snd_stream + MiniGB APU bridge
│   (extras/audio_processor/ for volume/mute, extras/audio_ring/ for buffering)
├── input.c / input.h          # Maple controller → joypad
├── rom_browser.c / rom_browser.h  # ROM/save file I/O and ROM library UI
├── cover.c / cover.h          # W555 box art + procedural placeholders
├── menu.c / menu.h            # Start, main, pause, and settings menus
├── settings.c / settings.h    # Persistent configuration
├── toast.c / toast.h          # Transient on-screen notifications
├── ui.c / ui.h                # 640x480 text UI for menus and browser
├── covers/boxart/             # xero/boxart CC0 cover art (.w555)
├── font8x8.h                  # Embedded ASCII font
├── meta/ip.txt                # IP.BIN template for disc builds
├── scripts/build-disc.sh      # ISO/CDI packaging helper
└── palette.c / palette.h      # DMG palette auto-assignment (from SDL example)
```

MiniGB APU is compiled from `examples/sdl2/minigb_apu/` (shared, not duplicated).
Master volume and mute run through `extras/audio_processor/` (MIT). APU output is
buffered through `extras/audio_ring/` (MIT), which primes a latency cushion,
silence-pads consumer underruns, and drops the oldest frames on overrun to keep
AICA fed without persistent crackle.
Config load/save uses `extras/ini_kv/` (MIT); browser device, view, and last ROM persist across sessions.

## Compile-Time Defines (SH-4)

```c
#define WALNUT_GB_IS_LITTLE_ENDIAN 1
#define WALNUT_GB_32BIT_DMA        1
#define WALNUT_GB_16BIT_DMA        0
#define WALNUT_GB_32BIT_ALIGNED    1
#define WALNUT_GB_USE_INTRINSICS   1
#define ENABLE_LCD                 1
#define ENABLE_SOUND               1
#define WALNUT_GB_12_COLOUR        1
```

## Memory Budget (Estimated)

| Allocation | Size |
|------------|------|
| `struct gb_s` | ~48–64 KB |
| ROM buffer | 32 KB – 4 MB |
| Cart RAM / save | 0 – 128 KB |
| Framebuffer RGB555 | 46 KB |
| Audio ring + stream buffers | ~144 KB (static) |
| PVR texture (256×256) | 128 KB |
| **Total typical** | **< 6 MB** of 16 MB |

---

## Phase 0 — Toolchain Smoke Test

- [ ] KOS "hello world" builds and runs on Flycast + hardware
- [ ] Confirm `sh-elf-gcc` compiles a TU that `#include`s `walnut_cgb.h`
- [ ] Source `environ.sh` from KOS install; verify `$KOS_BASE` and `$KOS_PORTS`

**Deliverable:** Developer environment documented in `README.md`.

---

## Phase 1 — Minimal Playable Core

- [x] `examples/dreamcast/` scaffold with KOS `Makefile`
- [x] ROM/RAM callbacks (`gb_rom_read`, `gb_cart_ram_read/write`, `gb_error`)
- [x] `lcd_draw_line` → RGB555 framebuffer → PVR sprite present
- [x] Maple controller → `gb->direct.joypad`
- [x] `gb_run_frame_dualfetch()` main loop at ~59.73 Hz
- [ ] Boot test: DMG title screen at full speed on Flycast/hardware

**Usage (dcload):**

```bash
dc-tool -x walnut-dc.elf /pc/roms/game.gb
```

**Deliverable:** Single ROM boots with graphics and controls.

---

## Phase 2 — Audio & Saves

- [x] MiniGB APU wired through `snd_stream`
- [x] Save/load `.sav` to FAT (`/pc`, `/sd`, `/cd`)
- [x] Error handler writes `recovery.sav` (plus normal `.sav` when path is known)
- [x] Autosave every 60 seconds during play
- [ ] Boot test: playable session with sound and persistent save

**Deliverable:** Full play session with audio and save persistence.

---

## Phase 3 — ROM Browser & Polish

- [x] Directory scanner for `/cd`, `/sd`, `/ide`, `/pc` (one subfolder level)
- [x] Simple text UI for ROM selection
- [x] Boot disc metadata (`IP.BIN` template, `scripts/build-disc.sh`)
- [x] Palette cycling (Y button) and fast-forward (triggers)
- [x] Frameskip toggle (Start + X)
- [x] Start+B returns to main menu when launched without ROM argument
- [x] Start screen and main menu (Continue, ROM Library, Settings, Controls, Exit)
- [x] Recent ROM history and DMG/GBC/Fav filter in ROM browser
- [x] ROM library favorites (Start+Y) and L/R letter jump
- [x] ZIP ROM loading (store/deflate, first `.gb`/`.gbc` member)
- [x] CDDA menu music (disc audio tracks; Settings toggle)
- [x] Config migration from `walnut-dc.cfg` to `pocketdc.cfg`
- [x] Pause menu with manual save/load (Start + Y)
- [x] Persistent settings (`pocketdc.cfg`) with live apply for video/audio
- [x] ROM library list/grid views with box art (xero/boxart CC0)
- [x] Scale modes, status bar HUD, volume/mute, audio buffer modes
- [x] Toast notifications and controls reference screen
- [x] Atomic `.sav` writes, pause load with `gb_reset()`, save error feedback
- [x] MBC3 RTC `.rtc` sidecar with wall-clock catch-up
- [x] Pause-menu erase save, 4× fast-forward (L+R), extra boot ROM search paths
- [x] MIT `gb_serialize` API and pause-menu Save/Load State (`.ss0`)
- [ ] Burn test: self-bootable CDI/GDI on hardware

**Deliverable:** Self-contained CDI/GDI image without PC assistance.

---

## Phase 4 — Release Hardening

- [ ] Test matrix: DMG, CGB, MBC1/2/3/5, RTC (Pokémon Gold/Silver)
- [ ] Hardware test: stock GD-ROM, IDE mod, SD adapter
- [ ] Compare save files against SDL frontend for same ROM
- [ ] Publish CDI/GDI to GitHub Releases

**Deliverable:** Reproducible release build with test notes.

---

## Controller Map

| GB button | Dreamcast |
|-----------|-----------|
| A | A |
| B | B |
| Start | Start |
| Select | X |
| D-Pad | D-Pad |

| Extra | Action |
|-------|--------|
| Start + A | Reset game |
| Start + Y | Pause menu |
| Start + B | Exit to main menu (menu mode) |
| Start + X | Toggle frameskip |
| Start + L | Cycle scale mode (does not fast-forward) |
| Y | Cycle palette |
| A+B+X+Y+Start | Quit to Dreamcast loader |
| L / R trigger | Fast-forward (2×; ignored while Start is held) |
| L + R trigger | Fast-forward 4× |

---

## Testing Strategy

| Layer | Method |
|-------|--------|
| Core accuracy | Host `make -C test` (core); `make -C test ci` runs `extras_test` for `ini_kv`, `audio_processor`, `audio_ring`, `zip_rom`, and `gb_serialize` |
| DC build | `sh-elf-gcc -Wall -Wextra` clean compile |
| Functional | cpu_instrs, dmg-acid2 via dcload |
| Game spot-checks | Tetris, Pokémon Blue, Oracle of Seasons, Shantae |
| Performance | Flycast FPS overlay; serial debug on hardware |

---

## Success Criteria

1. DMG and CGB games run at full speed without frameskip
2. Audio is stable (no persistent crackle)
3. Saves persist across sessions on FAT
4. User can select ROMs from disc/SD (Phase 3)
5. Build is reproducible from README + standard KOS toolchain

---

## References

- [dreamcast.wiki — Getting Started](https://dreamcast.wiki/Getting_Started_with_Dreamcast_development)
- [KallistiOS documentation](https://kos-docs.dreamcast.wiki/)
- SDL reference frontend: `examples/sdl2/walnut_sdl.c`
- Embedded reference: `examples/arduino/arduino.ino`
