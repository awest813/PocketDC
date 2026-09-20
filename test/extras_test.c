#include "minctest.h"

#define ENABLE_SOUND 0
#define ENABLE_LCD 1
#include "../walnut_cgb.h"

#include "../extras/audio_processor/audio_processor.h"
#include "../extras/audio_ring/audio_ring.h"
#include "../extras/ini_kv/ini_kv.h"
#include "../extras/zip_rom/zip_rom.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

static void test_ini_kv_get_int(void)
{
	int value;

	lequal(ini_kv_get_int("volume=75", "volume", &value), 1);
	lequal(value, 75);
	lequal(ini_kv_get_int("volume=75", "mute", &value), 0);
	lequal(ini_kv_get_int("volume_max=100", "volume", &value), 0);
	lequal(ini_kv_get_int("\tvolume\t=\t9", "volume", &value), 1);
	lequal(value, 9);
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

	lequal(ini_kv_get_string("favorite_rom_10=/sd/b.gb",
				 "favorite_rom_1", value, sizeof(value)),
	       0);
	lequal(ini_kv_get_string("favorite_rom_1=/sd/a.gb",
				 "favorite_rom_1", value, sizeof(value)),
	       1);
	lok(strcmp(value, "/sd/a.gb") == 0);
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

	samples[0] = 800;
	samples[1] = -800;
	audio_processor_set_volume(&proc, 0);
	audio_processor_process_s16_stereo(&proc, samples, 1);
	lequal(samples[0], 0);
	lequal(samples[1], 0);
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

static void test_audio_ring_wraparound(void)
{
	int16_t storage[8 * 2];
	int16_t src[5 * 2];
	int16_t more[4 * 2];
	int16_t dst[6 * 2];
	struct audio_ring ring;

	audio_ring_init(&ring, storage, 8, 0);
	fill_ramp(src, 1, 5);
	audio_ring_push(&ring, src, 5);
	lequal((int)audio_ring_pop(&ring, dst, 3), 3);
	lequal(dst[0], 1);
	lequal(dst[4], 3);

	fill_ramp(more, 10, 4);
	audio_ring_push(&ring, more, 4); /* write wraps; queued 2+4=6 */
	lequal((int)audio_ring_used(&ring), 6);
	lequal((int)audio_ring_pop(&ring, dst, 6), 6);
	lequal(dst[0], 4);
	lequal(dst[2], 5);
	lequal(dst[4], 10);
	lequal(dst[10], 13);
	lequal((int)ring.overruns, 0);
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

static void put_le16(uint8_t *p, uint16_t v)
{
	p[0] = (uint8_t)(v & 0xff);
	p[1] = (uint8_t)((v >> 8) & 0xff);
}

static void put_le32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)(v & 0xff);
	p[1] = (uint8_t)((v >> 8) & 0xff);
	p[2] = (uint8_t)((v >> 16) & 0xff);
	p[3] = (uint8_t)((v >> 24) & 0xff);
}

static void make_dummy_rom(uint8_t *rom, size_t len, const char *title)
{
	size_t i;

	memset(rom, 0, len);
	for (i = 0; title[i] != '\0' && i < 15; i++)
		rom[0x134 + i] = (uint8_t)title[i];
	rom[0x143] = 0x80;
	rom[0x147] = 0x01;
	rom[0x148] = 0x00;
}

static int deflate_raw(const uint8_t *in, size_t in_len, uint8_t *out, size_t out_cap,
		       size_t *out_len)
{
	z_stream stream;
	int ret;

	memset(&stream, 0, sizeof(stream));
	if (deflateInit2(&stream, Z_BEST_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8,
			 Z_DEFAULT_STRATEGY) != Z_OK)
		return -1;

	stream.next_in = (Bytef *)in;
	stream.avail_in = (uInt)in_len;
	stream.next_out = out;
	stream.avail_out = (uInt)out_cap;
	ret = deflate(&stream, Z_FINISH);
	*out_len = (size_t)stream.total_out;
	deflateEnd(&stream);
	return ret == Z_STREAM_END ? 0 : -1;
}

static int write_single_zip(const char *path, const char *name, const uint8_t *data,
			    size_t size, int method)
{
	FILE *f;
	uint8_t local[30];
	uint8_t central[46];
	uint8_t eocd[22];
	uint8_t comp[1024];
	const uint8_t *payload = data;
	size_t payload_len = size;
	uint32_t crc;
	uint16_t name_len;
	long cd_off;

	if (method == 8) {
		if (deflate_raw(data, size, comp, sizeof(comp), &payload_len) != 0)
			return -1;
		payload = comp;
	}

	crc = (uint32_t)crc32(0L, data, (uInt)size);
	name_len = (uint16_t)strlen(name);

	memset(local, 0, sizeof(local));
	put_le32(local, 0x04034b50);
	put_le16(local + 4, 20);
	put_le16(local + 8, (uint16_t)method);
	put_le32(local + 14, crc);
	put_le32(local + 18, (uint32_t)payload_len);
	put_le32(local + 22, (uint32_t)size);
	put_le16(local + 26, name_len);

	memset(central, 0, sizeof(central));
	put_le32(central, 0x02014b50);
	put_le16(central + 4, 20);
	put_le16(central + 6, 20);
	put_le16(central + 10, (uint16_t)method);
	put_le32(central + 16, crc);
	put_le32(central + 20, (uint32_t)payload_len);
	put_le32(central + 24, (uint32_t)size);
	put_le16(central + 28, name_len);

	f = fopen(path, "wb");
	if (!f)
		return -1;
	fwrite(local, 1, sizeof(local), f);
	fwrite(name, 1, name_len, f);
	fwrite(payload, 1, payload_len, f);
	cd_off = ftell(f);
	fwrite(central, 1, sizeof(central), f);
	fwrite(name, 1, name_len, f);

	memset(eocd, 0, sizeof(eocd));
	put_le32(eocd, 0x06054b50);
	put_le16(eocd + 8, 1);
	put_le16(eocd + 10, 1);
	put_le32(eocd + 12, (uint32_t)(46 + name_len));
	put_le32(eocd + 16, (uint32_t)cd_off);
	fwrite(eocd, 1, sizeof(eocd), f);
	fclose(f);
	return 0;
}

static void test_zip_rom_names(void)
{
	lok(zip_rom_path_is_zip("/sd/roms/game.zip"));
	lok(zip_rom_path_is_zip("GAME.ZIP"));
	lok(!zip_rom_path_is_zip("/sd/roms/game.gbc"));
	lok(zip_rom_name_is_gb("folder/Tetris.gb"));
	lok(zip_rom_name_is_gb("POCKET.GBC"));
	lok(!zip_rom_name_is_gb("readme.txt"));
	lok(!zip_rom_name_is_gb("folder/"));
}

static void test_zip_rom_stored_and_deflate(void)
{
	uint8_t rom[0x150];
	uint8_t header[0x150];
	uint8_t *extracted = NULL;
	size_t extracted_size = 0;
	size_t got = 0;
	char stored_path[] = "/tmp/pocketdc_stored.zip";
	char deflate_path[] = "/tmp/pocketdc_deflate.zip";
	char empty_path[] = "/tmp/pocketdc_empty.zip";

	make_dummy_rom(rom, sizeof(rom), "POCKETDC");
	lok(write_single_zip(stored_path, "pocket.gbc", rom, sizeof(rom), 0) == 0);
	lok(write_single_zip(deflate_path, "games/pocket.gb", rom, sizeof(rom), 8) == 0);
	lok(write_single_zip(empty_path, "notes.txt", rom, sizeof(rom), 0) == 0);

	lequal(zip_rom_read(stored_path, header, sizeof(header), &got), 0);
	lequal((int)got, (int)sizeof(header));
	lok(memcmp(header + 0x134, "POCKETDC", 8) == 0);
	lequal(header[0x143], 0x80);

	extracted = NULL;
	lequal(zip_rom_extract(deflate_path, &extracted, &extracted_size), 0);
	lequal((int)extracted_size, (int)sizeof(rom));
	lok(extracted != NULL);
	lok(memcmp(extracted, rom, sizeof(rom)) == 0);
	free(extracted);

	lequal(zip_rom_extract(empty_path, &extracted, &extracted_size), -1);

	remove(stored_path);
	remove(deflate_path);
	remove(empty_path);
}

static uint8_t ser_rom[0x8000];
static uint8_t ser_ram[0x2000];
static int ser_errors;

static uint8_t ser_rom_read(struct gb_s *gb, const uint_fast32_t addr)
{
	(void)gb;
	if (addr >= sizeof(ser_rom))
		return 0xFF;
	return ser_rom[addr];
}

static uint16_t ser_rom_read16(struct gb_s *gb, const uint_fast32_t addr)
{
	uint16_t lo = ser_rom_read(gb, addr);
	uint16_t hi = ser_rom_read(gb, addr + 1);

	return (uint16_t)(lo | (hi << 8));
}

static uint32_t ser_rom_read32(struct gb_s *gb, const uint_fast32_t addr)
{
	uint32_t v = ser_rom_read16(gb, addr);
	v |= ((uint32_t)ser_rom_read16(gb, addr + 2)) << 16;
	return v;
}

static uint8_t ser_ram_read(struct gb_s *gb, const uint_fast32_t addr)
{
	(void)gb;
	if (addr >= sizeof(ser_ram))
		return 0xFF;
	return ser_ram[addr];
}

static void ser_ram_write(struct gb_s *gb, const uint_fast32_t addr, const uint8_t val)
{
	(void)gb;
	if (addr < sizeof(ser_ram))
		ser_ram[addr] = val;
}

static void ser_error(struct gb_s *gb, const enum gb_error_e err, const uint16_t addr)
{
	(void)gb;
	(void)err;
	(void)addr;
	ser_errors++;
}

static void ser_make_rom(void)
{
	uint8_t x = 0;
	uint16_t i;

	memset(ser_rom, 0, sizeof(ser_rom));
	memset(ser_ram, 0, sizeof(ser_ram));
	memcpy(ser_rom + 0x0134, "SERTEST", 7);
	ser_rom[0x0147] = 0x03;
	ser_rom[0x0148] = 0x00;
	ser_rom[0x0149] = 0x02;
	for (i = 0x0134; i <= 0x014C; i++)
		x = (uint8_t)(x - ser_rom[i] - 1);
	ser_rom[0x014D] = x;
	ser_rom[0x0100] = 0x00;
	ser_rom[0x0101] = 0x18;
	ser_rom[0x0102] = (uint8_t)-2;
}

static int ser_init(struct gb_s *gb)
{
	ser_make_rom();
	ser_errors = 0;
	return gb_init(gb, ser_rom_read, ser_rom_read16, ser_rom_read32,
		       ser_ram_read, ser_ram_write, ser_error, NULL);
}

static void test_gb_serialize_roundtrip(void)
{
	struct gb_s gb;
	size_t size;
	uint8_t *buf;

	lequal(ser_init(&gb), GB_INIT_NO_ERROR);
	gb.cpu_reg.a = 0x42;
	gb.cpu_reg.pc.reg = 0x0150;
	gb.wram[0] = 0xAB;
	gb.vram[1] = 0xCD;
	gb.oam[2] = 0xEF;
	gb.hram_io[0x80] = 0x11;
	ser_ram[0] = 0x5A;
	gb.rtc_real.bytes[0] = 12;
	gb.selected_rom_bank = 1;
	gb.direct.frame_skip = true;

	size = gb_serialize_size(&gb);
	lok(size > 64);
	buf = (uint8_t *)malloc(size);
	lok(buf != NULL);
	lequal(gb_serialize(&gb, buf, size), GB_SERIALIZE_OK);

	gb.cpu_reg.a = 0;
	gb.cpu_reg.pc.reg = 0x0100;
	gb.wram[0] = 0;
	gb.vram[1] = 0;
	gb.oam[2] = 0;
	gb.hram_io[0x80] = 0;
	ser_ram[0] = 0;
	gb.rtc_real.bytes[0] = 0;
	gb.direct.frame_skip = false;
	gb.direct.priv = (void *)(uintptr_t)0x1;

	lequal(gb_deserialize(&gb, buf, size), GB_SERIALIZE_OK);
	lequal(gb.cpu_reg.a, 0x42);
	lequal(gb.cpu_reg.pc.reg, 0x0150);
	lequal(gb.wram[0], 0xAB);
	lequal(gb.vram[1], 0xCD);
	lequal(gb.oam[2], 0xEF);
	lequal(gb.hram_io[0x80], 0x11);
	lequal(ser_ram[0], 0x5A);
	lequal(gb.rtc_real.bytes[0], 12);
	lok(gb.direct.frame_skip);
	lok(gb.gb_rom_read == ser_rom_read);
	lok(gb.direct.priv == (void *)(uintptr_t)0x1);
	lequal(ser_errors, 0);

	buf[0] ^= 0xFF;
	lequal(gb_deserialize(&gb, buf, size), GB_SERIALIZE_ERROR_MAGIC);
	free(buf);
}

static void test_gb_serialize_rom_mismatch(void)
{
	struct gb_s gb;
	size_t size;
	uint8_t *buf;

	lequal(ser_init(&gb), GB_INIT_NO_ERROR);
	size = gb_serialize_size(&gb);
	buf = (uint8_t *)malloc(size);
	lok(buf != NULL);
	lequal(gb_serialize(&gb, buf, size), GB_SERIALIZE_OK);

	buf[8] ^= 0xFF;
	lequal(gb_deserialize(&gb, buf, size), GB_SERIALIZE_ERROR_ROM);
	lequal(gb_serialize(&gb, NULL, size), GB_SERIALIZE_ERROR_ARG);
	lequal((int)gb_serialize_size(NULL), 0);
	free(buf);
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
	lrun("audio_ring_wraparound", test_audio_ring_wraparound);
	lrun("audio_ring_reset", test_audio_ring_reset);
	lrun("zip_rom_names", test_zip_rom_names);
	lrun("zip_rom_stored_and_deflate", test_zip_rom_stored_and_deflate);
	lrun("gb_serialize_roundtrip", test_gb_serialize_roundtrip);
	lrun("gb_serialize_rom_mismatch", test_gb_serialize_rom_mismatch);
	lresults();
	return lfails != 0;
}
