/*
 * PocketDC Dreamcast frontend — AICA audio via snd_stream.
 * Copyright (c) 2025 Mr. Paul (https://github.com/Mr-PauI)
 * Licensed under the MIT License.
 */

#include <stdbool.h>
#include <string.h>

#include <kos.h>
#include <dc/sound/stream.h>

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

int dc_audio_init(void)
{
	unsigned int capacity;
	unsigned int target;

	audio_active = false;
	audio_processor_init(&processor);
	minigb_apu_audio_init(&apu);

	dc_audio_ring_bounds_for_mode(DC_AUDIO_BUFFER_NORMAL, &capacity, &target);
	audio_ring_init(&ring, ring_storage, capacity, target);

	snd_stream_init();
	stream = snd_stream_alloc(dc_audio_stream_callback, 0x4000);
	if (stream == SND_STREAM_INVALID)
		return -1;

	snd_stream_start(stream, AUDIO_SAMPLE_RATE, 1);
	audio_active = true;
	return 0;
}

void dc_audio_shutdown(void)
{
	audio_active = false;

	if (stream != SND_STREAM_INVALID) {
		snd_stream_stop(stream);
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
}

bool dc_audio_ready(void)
{
	return audio_active;
}

void dc_audio_frame(void)
{
	int16_t frame_buf[AUDIO_SAMPLES_TOTAL];

	if (!audio_active || stream == SND_STREAM_INVALID)
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
