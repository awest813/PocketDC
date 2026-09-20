/*
 * PocketDC Dreamcast frontend — AICA audio via snd_stream.
 * Copyright (c) 2025 Mr. Paul (https://github.com/Mr-PauI)
 * Licensed under the MIT License.
 */

#include <stdbool.h>
#include <string.h>

#include <kos.h>
#include <dc/cdrom.h>
#include <dc/sound/stream.h>
#include <dc/spu.h>
#ifndef CDROM_TOC
#define CDROM_TOC cd_toc_t
#endif

#include "../../extras/audio_processor/audio_processor.h"
#include "../../extras/audio_ring/audio_ring.h"

#define AUDIO_SAMPLE_RATE 44100
#define MINIGB_APU_AUDIO_FORMAT_S16SYS
#include "../sdl2/minigb_apu/minigb_apu.h"

#define DC_AUDIO_RING_MAX_FRAMES 32768
#define DC_AUDIO_STEREO_FRAME_BYTES (sizeof(int16_t) * AUDIO_CHANNELS)
#define DC_AUDIO_STREAM_MAX_FRAMES 4096

static struct minigb_apu_ctx apu;
static snd_stream_hnd_t stream = SND_STREAM_INVALID;
static bool audio_active;
static bool stream_running;
static bool in_game;
static bool menu_music_wanted = true;
static bool cdda_playing;
static bool cdda_paused;
static int cdda_first_track;
static int cdda_last_track;
static int16_t ring_storage[DC_AUDIO_RING_MAX_FRAMES * 2];
static struct audio_ring ring;
static struct audio_processor processor;
static int16_t stream_buf[DC_AUDIO_STREAM_MAX_FRAMES * 2] __attribute__((aligned(32)));

/*
 * Capacity sizes the worst-case backlog; target is the latency cushion the
 * buffer is primed to and tries to hold, trading delay for resistance to
 * underrun-driven crackle. Both are in stereo frames.
 */
static void dc_audio_ring_bounds_for_mode(enum dc_audio_buffer_mode mode,
					  unsigned int *capacity,
					  unsigned int *target)
{
	switch (mode) {
	case DC_AUDIO_BUFFER_LOW:
		*capacity = 4096;
		*target = AUDIO_SAMPLES;          /* ~1 frame of cushion */
		break;
	case DC_AUDIO_BUFFER_HIGH:
		*capacity = DC_AUDIO_RING_MAX_FRAMES;
		*target = AUDIO_SAMPLES * 4U;     /* ~4 frames of cushion */
		break;
	case DC_AUDIO_BUFFER_NORMAL:
	default:
		*capacity = 16384;
		*target = AUDIO_SAMPLES * 2U;     /* ~2 frames of cushion */
		break;
	}
}

static void *dc_audio_stream_callback(snd_stream_hnd_t hnd, int smp_req, int *smp_recv)
{
	unsigned int frames_requested = (unsigned int)smp_req / DC_AUDIO_STEREO_FRAME_BYTES;

	(void)hnd;

	if (frames_requested > DC_AUDIO_STREAM_MAX_FRAMES)
		frames_requested = DC_AUDIO_STREAM_MAX_FRAMES;

	/* Always hand back a full buffer; the ring silence-pads any shortfall. */
	audio_ring_pop(&ring, stream_buf, frames_requested);
	audio_processor_process_s16_stereo(&processor, stream_buf, frames_requested);

	*smp_recv = (int)(frames_requested * DC_AUDIO_STEREO_FRAME_BYTES);
	return stream_buf;
}

static int dc_cdda_volume_nibble(void)
{
	int volume;

	if (!menu_music_wanted || processor.muted)
		return 0;

	volume = ((int)processor.volume * 15 + 50) / 100;
	if (volume < 0)
		volume = 0;
	if (volume > 15)
		volume = 15;
	return volume;
}

static int dc_cdda_collect_tracks(int *first_out, int *last_out)
{
	CDROM_TOC toc;
	int session;
	int first_track;
	int last_track;
	int track;
	int first_audio = 0;
	int last_audio = 0;

	for (session = 0; session <= 1; session++) {
		first_audio = 0;
		last_audio = 0;
		if (cdrom_read_toc(&toc, session) != 0)
			continue;

		first_track = (int)TOC_TRACK(toc.first);
		last_track = (int)TOC_TRACK(toc.last);
		if (first_track < 1 || last_track > 99 || first_track > last_track)
			continue;

		for (track = first_track; track <= last_track; track++) {
			if ((TOC_CTRL(toc.entry[track - 1]) & 4) != 0)
				continue;
			if (first_audio == 0)
				first_audio = track;
			last_audio = track;
		}

		if (first_audio != 0) {
			*first_out = first_audio;
			*last_out = last_audio;
			return 0;
		}
	}

	return -1;
}

static void dc_cdda_apply_volume(void)
{
	const int volume = dc_cdda_volume_nibble();

	spu_cdda_volume(volume, volume);
}

static void dc_cdda_stop(void)
{
	if (!cdda_playing && !cdda_paused)
		return;

	cdrom_cdda_pause();
	cdda_playing = false;
	cdda_paused = false;
}

static void dc_cdda_start(void)
{
	if (in_game || !audio_active)
		return;

	dc_cdda_stop();
	if (dc_cdda_volume_nibble() == 0)
		return;
	if (dc_cdda_collect_tracks(&cdda_first_track, &cdda_last_track) != 0)
		return;

	dc_cdda_apply_volume();
	if (cdrom_cdda_play((uint32_t)cdda_first_track, (uint32_t)cdda_last_track,
			    15, CDDA_TRACKS) != 0)
		return;

	cdda_playing = true;
	cdda_paused = false;
}

static void dc_stream_stop(void)
{
	if (!stream_running || stream == SND_STREAM_INVALID)
		return;

	snd_stream_stop(stream);
	stream_running = false;
}

static int dc_stream_start(void)
{
	if (!audio_active || stream == SND_STREAM_INVALID)
		return -1;
	if (stream_running)
		return 0;

	snd_stream_start(stream, AUDIO_SAMPLE_RATE, 1);
	stream_running = true;
	return 0;
}

int dc_audio_init(void)
{
	unsigned int capacity;
	unsigned int target;

	audio_active = false;
	stream_running = false;
	in_game = false;
	cdda_playing = false;
	cdda_paused = false;
	menu_music_wanted = true;
	audio_processor_init(&processor);
	minigb_apu_audio_init(&apu);

	dc_audio_ring_bounds_for_mode(DC_AUDIO_BUFFER_NORMAL, &capacity, &target);
	audio_ring_init(&ring, ring_storage, capacity, target);

	snd_stream_init();
	stream = snd_stream_alloc(dc_audio_stream_callback, 0x4000);
	if (stream == SND_STREAM_INVALID)
		return -1;

	audio_active = true;
	return 0;
}

void dc_audio_shutdown(void)
{
	in_game = false;
	dc_cdda_stop();
	dc_stream_stop();
	audio_active = false;

	if (stream != SND_STREAM_INVALID) {
		snd_stream_destroy(stream);
		stream = SND_STREAM_INVALID;
	}
}

void dc_audio_configure(uint8_t volume, bool muted,
			enum dc_audio_buffer_mode buffer_mode)
{
	unsigned int capacity;
	unsigned int target;

	audio_processor_set_volume(&processor, volume);
	audio_processor_set_muted(&processor, muted);

	dc_audio_ring_bounds_for_mode(buffer_mode, &capacity, &target);

	if (capacity != ring.capacity || target != ring.target) {
		audio_ring_init(&ring, ring_storage, capacity, target);
		audio_processor_reset(&processor);
	}

	if (!in_game) {
		dc_cdda_apply_volume();
		if (dc_cdda_volume_nibble() == 0)
			dc_cdda_stop();
		else if (!cdda_playing)
			dc_cdda_start();
	}
}

void dc_audio_set_menu_music(bool enabled)
{
	menu_music_wanted = enabled;
	if (in_game)
		return;

	if (enabled)
		dc_cdda_start();
	else
		dc_cdda_stop();
}

void dc_audio_enter_menu(void)
{
	in_game = false;
	dc_stream_stop();
	dc_cdda_start();
}

void dc_audio_enter_game(void)
{
	dc_cdda_stop();
	in_game = true;
	audio_ring_reset(&ring);
	audio_processor_reset(&processor);
	dc_stream_start();
}

void dc_audio_cdda_hold(void)
{
	if (!cdda_playing || cdda_paused)
		return;

	if (cdrom_cdda_pause() == 0)
		cdda_paused = true;
}

void dc_audio_cdda_release(void)
{
	if (!cdda_playing || !cdda_paused)
		return;

	if (cdrom_cdda_resume() != 0) {
		cdda_playing = false;
		cdda_paused = false;
		dc_cdda_start();
		return;
	}

	cdda_paused = false;
}

bool dc_audio_ready(void)
{
	return audio_active;
}

void dc_audio_frame(void)
{
	int16_t frame_buf[AUDIO_SAMPLES_TOTAL];

	if (!audio_active || !stream_running || stream == SND_STREAM_INVALID)
		return;

	minigb_apu_audio_callback(&apu, frame_buf);
	audio_ring_push(&ring, frame_buf, AUDIO_SAMPLES);
	snd_stream_poll(stream);
}

uint8_t dc_audio_read(uint16_t addr)
{
	return minigb_apu_audio_read(&apu, addr);
}

void dc_audio_write(uint16_t addr, uint8_t val)
{
	minigb_apu_audio_write(&apu, addr, val);
}
