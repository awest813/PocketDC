# PocketDC

<img width="128" height="128" alt="PocketDC logo" src="https://github.com/user-attachments/assets/b071aaef-73ba-4f36-bcf8-06964ae2069f" align="right" />

PocketDC is a Dreamcast-only fork of Walnut-CGB. It packages the Game Boy / Game Boy Color emulator core with a KallistiOS frontend for Sega Dreamcast homebrew.

**Current version:** 1.3.1

## What you get

- Dreamcast-focused Game Boy and Game Boy Color emulation
- KallistiOS frontend with ROM browser, direct loading, and cover art
- Maple controller input, PVR video output, and AICA audio
- Optional boot-disc packaging for CD-R, GD-ROM, SD, IDE, and dcload workflows
- Bundled MiniGB APU for Dreamcast audio output
- MIT-licensed shared modules: `extras/audio_processor/` (volume, DC block, mute fade), `extras/audio_ring/` (latency-cushioned stereo buffering), and `extras/ini_kv/` (config)

Sound emulation is not built in. When `ENABLE_SOUND` is set, PocketDC uses the bundled MiniGB APU in `examples/sdl2/minigb_apu`, with post-processing from `extras/audio_processor/` (also used by the SDL reference frontend).

## Quick start

1. Build the [KallistiOS](https://github.com/KallistiOS/KallistiOS) toolchain and SDK.
2. Source the KOS environment:

```bash
source /opt/toolchains/dc/kos/environ.sh
```

Adjust the path to match your install.

3. Build the Dreamcast example:

```bash
cd examples/dreamcast
make
```

4. Run it with dcload:

```bash
dc-tool -x walnut-dc.elf
dc-tool -x walnut-dc.elf /pc/roms/tetris.gb
```

See [`examples/dreamcast/README.md`](examples/dreamcast/README.md) for controls, disc building, and settings.

## Highlights

- ROM library with list/grid browsing, recent ROMs, and DMG/GBC filters
- **Continue** last played game from the main menu
- Per-game save support
- Live settings for palette, video mode, scale, frameskip, and audio
- Persistent `pocketdc.cfg` (migrates from legacy `walnut-dc.cfg`)
- Self-boot disc image support
- Optional boot ROM support

## License

MIT License — see [`LICENSE`](LICENSE).
