# CDDA menu music

PocketDC plays the disc's **audio tracks** in menus (DreamShell-style). Game Boy sound takes over while a ROM is running.

## Audio files

Put 44.1 kHz stereo 16-bit PCM `.wav` files (or raw CD-DA `.raw`) in this directory. They are **not** copied into the data track; `scripts/build-disc.sh` passes them to `mkdcdisc -c` when that tool is on `PATH`.

Do not ship copyrighted music. Short original or CC0 loops are fine.

Suggested names so they sort in track order:

```
meta/cdda/track02.wav
meta/cdda/track03.wav
```

Track 1 is the data track. These files become tracks 2+.

## Without mkdcdisc

A data-only ISO/CDI still boots. Menu music stays silent until you burn a mixed-mode disc that actually contains CDDA tracks (GDEMU/ODE images that include audio tracks also work).

## Settings

**Menu music** in Settings (default on). Volume and mute apply to CDDA in menus. Playback pauses while the ROM library scans `/cd` so the GD-ROM can read files.
