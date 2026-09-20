# PocketDC Dreamcast Example

KallistiOS frontend for running PocketDC on the Sega Dreamcast.

## Prerequisites

1. Build the [KallistiOS](https://github.com/KallistiOS/KallistiOS) toolchain and SDK (include **zlib** from kos-ports; the ELF links `-lz` for ZIP ROMs).
2. Source the KOS environment:

```bash
source /opt/toolchains/dc/kos/environ.sh
```

Adjust the path to match your install. See [dreamcast.wiki](https://dreamcast.wiki/Getting_Started_with_Dreamcast_development).

## Build

```bash
cd examples/dreamcast
make
```

This produces `walnut-dc.elf`.

## Run

### Main menu (no arguments)

```bash
dc-tool -x walnut-dc.elf
```

Shows a start screen, then the main menu with **Continue** (when the last played ROM is still available), **ROM Library**, **Settings**, **Controls**, **About**, and **Exit**. Exit asks for confirmation. Hold **A+B+X+Y+Start** on any screen to return to the Dreamcast loader.

### ROM library

From the main menu, choose **ROM Library**. Scans these paths and **one level of subfolders** (press **B** to cycle devices). `covers`, `boxart`, and `saves` directories are skipped. `.gb`, `.gbc`, and `.zip` files are listed (the first `.gb`/`.gbc` member in a ZIP is used; store or deflate):

| Path | Typical source |
|------|----------------|
| `/cd/roms`, `/cd` | GD-ROM / burned disc |
| `/sd/roms`, `/sd` | SD adapter |
| `/ide/roms`, `/ide` | IDE/GDEMU/ODE |
| `/pc` | dcload transfer target |

Browser controls (D-pad or analog stick; hold to repeat). Recent ROMs appear at the top (`*` in list view, red corner in grid). Favorites are marked `+` in list view and a yellow corner in grid. L+R cycles All / DMG / GBC / Fav. L or R jumps by title letter. Start+Y pins or unpins a favorite (up to 16, stored in `pocketdc.cfg`):

| Button | Action |
|--------|--------|
| Up / Down | Select ROM |
| Left / Right | Page up / down |
| A | Load selected ROM |
| B | Next device/path |
| Start | Refresh list |
| Y | Toggle list / grid view |
| Start + Y | Pin or unpin favorite |
| L / R | Jump to previous / next letter |
| L + R | Cycle All / DMG / GBC / Fav |
| X | Back to main menu |

The header shows a friendly device name (for example **GD-ROM** or **SD Card**) plus the scan path.

- **List view** — ROM titles from the cartridge header, with a large cover preview on the right
- **Grid view** — 5×3 cover grid (procedural placeholders or your own art)
- **[SAV]** when a matching `.sav` exists (same basename as the `.gb`/`.gbc`/`.zip`)
- **[ZIP]** in the preview when the library entry is a compressed ROM
- **Box art** from [xero/boxart](https://github.com/xero/boxart) (CC0) for matching NoIntro ROM names — see [covers/README.md](covers/README.md)
- Optional overrides: `covers/ROMNAME.w555`

### Direct ROM load (dcload)

```bash
dc-tool -x walnut-dc.elf /pc/roms/tetris.gb
```

Optional explicit save path:

```bash
dc-tool -x walnut-dc.elf /pc/roms/tetris.gb /pc/roms/tetris.sav
```

### Flycast

Load `walnut-dc.elf` with a ROM path argument if your loader supports argv, or use dcload/IP loading.

## In-Game Controls

| GB | Dreamcast |
|----|-----------|
| A | A |
| B | B |
| Start | Start |
| Select | X |
| D-Pad | D-Pad or analog stick |

| Extra | Action |
|-------|--------|
| Start + Y | Pause menu (save/load; Main Menu when launched from the menu) |
| Start + A | Reset game |
| Start + B | Return to main menu (menu mode) or exit (direct load) |
| Y | Cycle palette |
| Start + X | Toggle frameskip |
| Start + L | Cycle scale mode |
| L / R trigger | Fast-forward (2×) |

## Settings

Accessible from the main menu or pause menu. Video and audio changes apply immediately while the menu is open. Options, recents, and favorites are saved to `pocketdc.cfg` on the first writable path (`/pc`, `/sd`, `/ide`, or `/cd`). Legacy `walnut-dc.cfg` files are still read if present.

| Setting | Description |
|---------|-------------|
| Palette | DMG colour palette (named presets) |
| Video output | Auto, VGA 640×480, or TV 640×480 (uses `vid_check_cable()`) |
| Scale mode | 3× integer, widescreen, 4× integer, or full screen |
| Status bar | In-game HUD with title, scale, and volume |
| Frameskip | Skip LCD updates for speed |
| Autosave | Periodic battery-RAM save during play |
| Autosave interval | Seconds between autosaves (10–300) |
| Volume | Master audio level (0–100%); press A on this row to mute |
| Menu music | Play disc CDDA tracks in menus (needs audio tracks on the disc) |
| Audio buffer | Low latency, normal, or stable buffering |

## Boot Disc (CDI/GDI)

1. Build the ELF: `make`
2. Package a disc image:

```bash
./scripts/build-disc.sh
```

3. Copy homebrew `.gb` / `.gbc` / `.zip` ROMs into `disc-build/roms/` before burning.
4. Box art is bundled automatically from `covers/boxart/` when you run `build-disc.sh`.
5. Refresh art from upstream: `./scripts/import-boxart.sh` or `make -f Makefile.covers fetch-covers`
6. Fetch covers only for ROMs on the disc: `make -f Makefile.covers fetch-roms ROMS_DIR=disc-build/roms`
7. Optional **menu CDDA**: 44.1 kHz stereo WAV files in `meta/cdda/` plus `mkdcdisc` on `PATH` (see [meta/cdda/README.md](meta/cdda/README.md)).
8. Burn `disc-build/walnut-dc.iso` or `disc-build/walnut-dc.cdi`.

Disc metadata is defined in `meta/ip.txt` (processed by KOS `makeip`).

## Optional Files

- `dmg_boot.bin` — DMG boot ROM in the working directory (optional)

## Status

See [PHASED_IMPLEMENTATION.md](PHASED_IMPLEMENTATION.md) for the port roadmap and checklist.

Phase 3 (ROM browser + disc packaging) is implemented. Phase 4 hardware validation remains.

## UX Features

- **Controls** screen in the main menu
- **Toast messages** for palette changes, frameskip, fast-forward, and autosave
- **Save/load confirmations** in the pause menu
- **Loading screen** when starting a ROM from the browser
- **Save indicators** in the ROM library
- **Scale modes** including widescreen (640×432) and full-screen stretch
- **Status bar** HUD during gameplay
- **Audio controls** for volume, mute, and buffer size
- **VGA mode** with auto cable detection (VGA box vs TV)
- **Cover-art ROM picker** with list/grid views, ROM counts, mapper/size info, recents, favorites, letter jump, ZIP ROMs, and bundled [xero/boxart](https://github.com/xero/boxart) GB/GBC art
- **CDDA menu music** when the disc has audio tracks (Settings: Menu music)
- **About** credits screen and Dreamcast A+B+X+Y+Start quit combo
- **Live settings** for video output, scale, and audio while browsing options

## Licensing

PocketDC is MIT licensed. Shared MIT modules live in `extras/audio_processor/` (volume/mute/DC block), `extras/audio_ring/` (audio buffering), `extras/ini_kv/` (config I/O), and `extras/zip_rom/` (ZIP Game Boy ROMs). MiniGB APU has its own license in `examples/sdl2/minigb_apu/LICENSE`. KOS requires attribution in distributed binaries. ZIP inflate uses zlib.

Do not ship copyrighted ROMs with homebrew releases.
