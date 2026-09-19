#include "minctest.h"

#include "../extras/audio_processor/audio_processor.h"
#include "../extras/audio_ring/audio_ring.h"
#include "../extras/ini_kv/ini_kv.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static void test_ini_kv_get_int(void)
{
	int value;

	lequal(ini_kv_get_int("volume=75", "volume", &value), 1);
	lequal(value, 75);
	lequal(ini_kv_get_int("volume=75", "mute", &value), 0);
	lequal(ini_kv_get_int("volume_max=100", "volume", &value), 0);
	lequal(ini_kv_get_int("  volume = 42", "volume", &value), 1);
	lequal(value, 42);
}

static void test_ini_kv_get_string(void)
{
	char value[32];

	lequal(ini_kv_get_string("last_rom_path=/sd/roms/tetris.gb",
				 "last_rom_path", value, sizeof(value)),
	       1);
	lok(strcmp(value, "/sd/roms/tetris.gb") == 0);

	memset(value, 0, sizeof(value));
	lequal(ini_kv_get_string("last_rom_path=/pc/foo.gb  ",
				 "last_rom_path", value, sizeof(value)),
	       1);
	lok(strcmp(value, "/pc/foo.gb") == 0);

	lequal(ini_kv_get_string("last_rom_path_alt=/sd/x.gb",
				 "last_rom_path", value, sizeof(value)),
	       0);
}

static void test_ini_kv_parse_bool(void)
{
	bool value;

	lequal(ini_kv_parse_bool("1", &value), 1);
	lok(value);
	lequal(ini_kv_parse_bool("true", &value), 1);
	lok(value);
	lequal(ini_kv_parse_bool("no", &value), 1);
	lok(!value);
	lequal(ini_kv_parse_bool("maybe", &value), 0);
}

static void test_ini_kv_fprint_roundtrip(void)
{
	char line[64];
	FILE *f;

	f = tmpfile();
	lok(f != NULL);
	ini_kv_fprint_int(f, "browser_filter", 2);
	ini_kv_fprint_bool(f, "muted", true);
	ini_kv_fprint_string(f, "last_rom_path", "/cd/roms/game.gbc");
	rewind(f);

	lok(fgets(line, sizeof(line), f) != NULL);
	{
		int value;

		lequal(ini_kv_get_int(line, "browser_filter", &value), 1);
		lequal(value, 2);
	}

	lok(fgets(line, sizeof(line), f) != NULL);
	{
		int value;

		lequal(ini_kv_get_int(line, "muted", &value), 1);
		lequal(value, 1);
	}

	lok(fgets(line, sizeof(line), f) != NULL);
	{
		char value[64];

		lequal(ini_kv_get_string(line, "last_rom_path", value, sizeof(value)),
		       1);
		lok(strcmp(value, "/cd/roms/game.gbc") == 0);
	}

	fclose(f);
}

static void test_audio_processor_volume(void)
{
	struct audio_processor proc;
	int16_t samples[2] = { 1000, -1000 };

	audio_processor_init(&proc);
	audio_processor_set_volume(&proc, 50);
	audio_processor_process_s16_stereo(&proc, samples, 1);

	lequal(samples[0], 500);
	lequal(samples[1], -500);
}

static void test_audio_processor_mute_fade(void)
{
	struct audio_processor proc;
	int16_t samples[2];
	unsigned int frame;

	audio_processor_init(&proc);
	samples[0] = 8000;
	samples[1] = -8000;
	audio_processor_set_muted(&proc, true);

	for (frame = 0; frame < AUDIO_PROCESSOR_FADE_SAMPLES; frame++) {
		samples[0] = 8000;
		samples[1] = -8000;
		audio_processor_process_s16_stereo(&proc, samples, 1);
	}

	lequal(samples[0], 0);
	lequal(samples[1], 0);
}

static void test_audio_processor_reset(void)
{
	struct audio_processor proc;
	int16_t samples[2] = { 0, 0 };

	audio_processor_init(&proc);
	proc.hp_state[0] = 5000;
	proc.hp_state[1] = -5000;
	audio_processor_process_s16_stereo(&proc, samples, 1);
	lok(samples[0] != 0 || samples[1] != 0);

	audio_processor_reset(&proc);
	samples[0] = 0;
	samples[1] = 0;
	audio_processor_process_s16_stereo(&proc, samples, 1);
	lequal(samples[0], 0);
	lequal(samples[1], 0);
}

/* Build `count` stereo frames with both channels set to base, base+1, ... */
static void fill_ramp(int16_t *frames, int16_t base, unsigned int count)
{
	unsigned int i;

	for (i = 0; i < count; i++) {
		frames[i * 2] = (int16_t)(base + (int16_t)i);
		frames[i * 2 + 1] = (int16_t)(base + (int16_t)i);
	}
}

static void test_audio_ring_prime(void)
{
	int16_t storage[8 * 2];
	int16_t dst[8 * 2];
	struct audio_ring ring;

	audio_ring_init(&ring, storage, 8, 2);
	/* Primed cushion counts as queued silence, not an underrun. */
	lequal((int)audio_ring_used(&ring), 2);
	lequal((int)audio_ring_pop(&ring, dst, 2), 2);
	lequal((int)ring.underruns, 0);
	lequal(dst[0], 0);
	lequal(dst[3], 0);
	lequal((int)audio_ring_used(&ring), 0);
}

static void test_audio_ring_push_pop(void)
{
	int16_t storage[8 * 2];
	int16_t src[3 * 2];
	int16_t dst[3 * 2];
	struct audio_ring ring;

	audio_ring_init(&ring, storage, 8, 0);
	fill_ramp(src, 1, 3);
	audio_ring_push(&ring, src, 3);
	lequal((int)audio_ring_used(&ring), 3);

	lequal((int)audio_ring_pop(&ring, dst, 3), 3);
	lequal(dst[0], 1);
	lequal(dst[2], 2);
	lequal(dst[4], 3);
	lequal((int)audio_ring_used(&ring), 0);
	lequal((int)ring.overruns, 0);
	lequal((int)ring.underruns, 0);
}

static void test_audio_ring_underrun(void)
{
	int16_t storage[8 * 2];
	int16_t src[2 * 2];
	int16_t dst[4 * 2];
	struct audio_ring ring;

	audio_ring_init(&ring, storage, 8, 0);
	fill_ramp(src, 5, 2);
	audio_ring_push(&ring, src, 2);

	/* Ask for more than is queued: real prefix returned, tail silenced. */
	lequal((int)audio_ring_pop(&ring, dst, 4), 2);
	lequal(dst[0], 5);
	lequal(dst[2], 6);
	lequal(dst[4], 0);
	lequal(dst[6], 0);
	lequal((int)ring.underruns, 2);
}

static void test_audio_ring_overrun(void)
{
	int16_t storage[8 * 2];
	int16_t src[5 * 2];
	int16_t more[4 * 2];
	int16_t dst[7 * 2];
	struct audio_ring ring;

	audio_ring_init(&ring, storage, 8, 0);
	fill_ramp(src, 1, 5);
	audio_ring_push(&ring, src, 5);
	fill_ramp(more, 6, 4);
	audio_ring_push(&ring, more, 4); /* free is 2, so drop 2 oldest (1,2) */

	lequal((int)ring.overruns, 2);
	lequal((int)audio_ring_used(&ring), 7);
	lequal((int)audio_ring_pop(&ring, dst, 7), 7);
	lequal(dst[0], 3);
	lequal(dst[12], 9);
}

static void test_audio_ring_oversized_block(void)
{
	int16_t storage[8 * 2];
	int16_t src[10 * 2];
	int16_t dst[7 * 2];
	struct audio_ring ring;

	audio_ring_init(&ring, storage, 8, 0);
	fill_ramp(src, 1, 10); /* block larger than capacity: keep newest tail */
	audio_ring_push(&ring, src, 10);

	lequal((int)ring.overruns, 3);
	lequal((int)audio_ring_used(&ring), 7);
	lequal((int)audio_ring_pop(&ring, dst, 7), 7);
	lequal(dst[0], 4);
	lequal(dst[12], 10);
}

static void test_audio_ring_reset(void)
{
	int16_t storage[8 * 2];
	int16_t src[3 * 2];
	struct audio_ring ring;

	audio_ring_init(&ring, storage, 8, 2);
	fill_ramp(src, 1, 3);
	audio_ring_push(&ring, src, 3);
	lequal((int)audio_ring_used(&ring), 5);

	audio_ring_reset(&ring);
	lequal((int)audio_ring_used(&ring), 2); /* re-primed to target cushion */
}

int main(void)
{
	lrun("ini_kv_get_int", test_ini_kv_get_int);
	lrun("ini_kv_get_string", test_ini_kv_get_string);
	lrun("ini_kv_parse_bool", test_ini_kv_parse_bool);
	lrun("ini_kv_fprint_roundtrip", test_ini_kv_fprint_roundtrip);
	lrun("audio_processor_volume", test_audio_processor_volume);
	lrun("audio_processor_mute_fade", test_audio_processor_mute_fade);
	lrun("audio_processor_reset", test_audio_processor_reset);
	lrun("audio_ring_prime", test_audio_ring_prime);
	lrun("audio_ring_push_pop", test_audio_ring_push_pop);
	lrun("audio_ring_underrun", test_audio_ring_underrun);
	lrun("audio_ring_overrun", test_audio_ring_overrun);
	lrun("audio_ring_oversized_block", test_audio_ring_oversized_block);
	lrun("audio_ring_reset", test_audio_ring_reset);
	lresults();
	return lfails != 0;
}
