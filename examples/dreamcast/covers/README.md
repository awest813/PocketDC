# ROM Cover Art

PocketDC shows cover art in the ROM picker using Dreamcast-native `.w555` thumbnails (96×96 RGB555).

## Bundled art (Game Boy / Game Boy Color)

This tree includes converted cover art from **[xero/boxart](https://github.com/xero/boxart)** (CC0 / public domain):

```
covers/boxart/GB/*.w555
covers/boxart/GBC/*.w555
```

Art is matched to ROMs by **NoIntro-style filename** (without the extension):

```
roms/Tetris (World) (Rev 1).gb
covers/boxart/GB/Tetris (World) (Rev 1).w555
```

Refresh from upstream:

```bash
pip install pillow

# Full catalog (sparse git clone — fast bulk import)
./scripts/import-boxart.sh

# Incremental HTTP sync (no git clone; downloads only missing .w555)
./scripts/import-boxart.sh --sync-remote
# or: python3 scripts/fetch-covers.py --sync-all
```

Fetch covers for ROMs you already have (downloads only matching titles):

```bash
./scripts/import-boxart.sh --roms-dir disc-build/roms
# or: python3 scripts/fetch-covers.py --roms-dir /path/to/roms
```

Makefile shortcuts (no KOS required):

```bash
make -f Makefile.covers fetch-covers
make -f Makefile.covers fetch-roms ROMS_DIR=disc-build/roms
```

## Lookup order

For each ROM, PocketDC tries:

1. `covers/ROMNAME.w555` — manual override
2. `covers/boxart/GBC/ROMNAME.w555` or `covers/GBC/ROMNAME.w555` (GBC ROMs)
3. `covers/boxart/GB/ROMNAME.w555` or `covers/GB/ROMNAME.w555`
4. Procedural placeholder (color + initials) if nothing matches

## Layout on hardware

```
/cd/roms/Game Name (USA).gbc
/cd/covers/boxart/GBC/Game Name (USA).w555
```

`build-disc.sh` copies `covers/boxart/` onto disc images automatically.

## Create your own `.w555`

```bash
pip install pillow
./scripts/convert-cover.py my-cover.png custom.w555
```

## Credits

- Box art images: [xero/boxart](https://github.com/xero/boxart) (CC0), sourced from [ScreenScraper](https://screenscraper.fr)
- `.w555` conversion and ROM matching: PocketDC Dreamcast frontend
