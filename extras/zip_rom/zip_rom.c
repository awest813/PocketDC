/*
 * PocketDC / Walnut-CGB — ZIP Game Boy ROM extractor.
 * Copyright (c) 2025 Mr. Paul (https://github.com/Mr-PauI)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "zip_rom.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <zlib.h>

#define ZIP_LOCAL_SIG   0x04034b50u
#define ZIP_CENTRAL_SIG 0x02014b50u
#define ZIP_EOCD_SIG    0x06054b50u
#define ZIP_METHOD_STORE   0
#define ZIP_METHOD_DEFLATE 8

static uint16_t zip_le16(const uint8_t *p)
{
	return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t zip_le32(const uint8_t *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
	       ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static const char *zip_basename(const char *name)
{
	const char *slash;
	const char *bslash;

	if (!name)
		return "";

	slash = strrchr(name, '/');
	bslash = strrchr(name, '\\');
	if (bslash && (!slash || bslash > slash))
		slash = bslash;

	return slash ? slash + 1 : name;
}

bool zip_rom_path_is_zip(const char *path)
{
	size_t len;

	if (!path)
		return false;

	len = strlen(path);
	return len >= 4 && strcasecmp(path + len - 4, ".zip") == 0;
}

bool zip_rom_name_is_gb(const char *name)
{
	const char *base;
	size_t len;

	base = zip_basename(name);
	if (base[0] == '\0' || base[0] == '.')
		return false;

	len = strlen(base);
	if (len >= 3 && strcasecmp(base + len - 3, ".gb") == 0)
		return true;
	if (len >= 4 && strcasecmp(base + len - 4, ".gbc") == 0)
		return true;

	return false;
}

static int zip_find_eocd(FILE *f, long size, uint8_t eocd[22], long *eocd_off)
{
	long start;
	long pos;
	uint8_t buf[256];

	if (size < 22)
		return -1;

	start = size - 22;
	if (start > 65535)
		start = size - 22 - 65535;
	if (start < 0)
		start = 0;

	for (pos = size - 22; pos >= start; pos--) {
		if (fseek(f, pos, SEEK_SET) != 0)
			return -1;
		if (fread(buf, 1, 22, f) != 22)
			return -1;
		if (zip_le32(buf) == ZIP_EOCD_SIG) {
			memcpy(eocd, buf, 22);
			*eocd_off = pos;
			return 0;
		}
		if (pos == 0)
			break;
	}

	return -1;
}

static int zip_inflate_into(FILE *f, size_t comp_size, uint8_t *out,
			    size_t out_cap, size_t *written)
{
	z_stream stream;
	uint8_t in[512];
	size_t remaining = comp_size;
	int ret;

	memset(&stream, 0, sizeof(stream));
	if (inflateInit2(&stream, -MAX_WBITS) != Z_OK)
		return -1;

	stream.next_out = out;
	stream.avail_out = (uInt)out_cap;

	while (remaining > 0 && stream.avail_out > 0) {
		size_t chunk = remaining > sizeof(in) ? sizeof(in) : remaining;

		if (fread(in, 1, chunk, f) != chunk) {
			inflateEnd(&stream);
			return -1;
		}

		remaining -= chunk;
		stream.next_in = in;
		stream.avail_in = (uInt)chunk;

		do {
			ret = inflate(&stream, Z_NO_FLUSH);
			if (ret == Z_STREAM_END)
				goto done;
			if (ret != Z_OK) {
				inflateEnd(&stream);
				return -1;
			}
		} while (stream.avail_in > 0 && stream.avail_out > 0);
	}

done:
	*written = (size_t)stream.total_out;
	inflateEnd(&stream);
	return 0;
}

struct zip_member
{
	uint16_t method;
	uint16_t flags;
	uint32_t comp_size;
	uint32_t uncomp_size;
	uint32_t local_off;
};

static int zip_locate_rom_member(FILE *f, struct zip_member *member)
{
	uint8_t eocd[22];
	uint8_t hdr[46];
	uint8_t name[256];
	long eocd_off = 0;
	uint16_t entries;
	uint32_t cd_size;
	uint32_t cd_off;
	uint32_t consumed;
	uint16_t i;

	{
		long size;

		if (fseek(f, 0, SEEK_END) != 0)
			return -1;
		size = ftell(f);
		if (zip_find_eocd(f, size, eocd, &eocd_off) != 0)
			return -1;
	}

	entries = zip_le16(eocd + 8);
	if (entries == 0)
		entries = zip_le16(eocd + 10);
	cd_size = zip_le32(eocd + 12);
	cd_off = zip_le32(eocd + 16);
	if (entries == 0 || cd_size == 0)
		return -1;

	if (fseek(f, (long)cd_off, SEEK_SET) != 0)
		return -1;

	consumed = 0;
	for (i = 0; i < entries && consumed + 46 <= cd_size; i++) {
		uint16_t name_len;
		uint16_t extra_len;
		uint16_t comment_len;

		if (fread(hdr, 1, 46, f) != 46)
			return -1;
		if (zip_le32(hdr) != ZIP_CENTRAL_SIG)
			return -1;

		name_len = zip_le16(hdr + 28);
		extra_len = zip_le16(hdr + 30);
		comment_len = zip_le16(hdr + 32);
		consumed += 46u + name_len + extra_len + comment_len;

		if (name_len >= sizeof(name)) {
			if (fseek(f, (long)name_len + extra_len + comment_len, SEEK_CUR) != 0)
				return -1;
			continue;
		}

		if (name_len > 0 && fread(name, 1, name_len, f) != name_len)
			return -1;
		name[name_len] = '\0';

		if (fseek(f, (long)extra_len + comment_len, SEEK_CUR) != 0)
			return -1;

		if (name_len == 0 || name[name_len - 1] == '/')
			continue;
		if (!zip_rom_name_is_gb((const char *)name))
			continue;

		member->flags = zip_le16(hdr + 8);
		member->method = zip_le16(hdr + 10);
		member->comp_size = zip_le32(hdr + 20);
		member->uncomp_size = zip_le32(hdr + 24);
		member->local_off = zip_le32(hdr + 42);

		if ((member->flags & 1u) != 0)
			continue;
		if (member->uncomp_size == 0 ||
		    member->uncomp_size == 0xffffffffu ||
		    member->uncomp_size > ZIP_ROM_MAX_SIZE)
			continue;
		if (member->method != ZIP_METHOD_STORE &&
		    member->method != ZIP_METHOD_DEFLATE)
			continue;

		return 0;
	}

	return -1;
}

static int zip_seek_member_data(FILE *f, const struct zip_member *member)
{
	uint8_t local[30];
	uint16_t name_len;
	uint16_t extra_len;

	if (fseek(f, (long)member->local_off, SEEK_SET) != 0)
		return -1;
	if (fread(local, 1, 30, f) != 30)
		return -1;
	if (zip_le32(local) != ZIP_LOCAL_SIG)
		return -1;

	name_len = zip_le16(local + 26);
	extra_len = zip_le16(local + 28);
	if (fseek(f, (long)name_len + extra_len, SEEK_CUR) != 0)
		return -1;

	return 0;
}

int zip_rom_read(const char *zip_path, uint8_t *buf, size_t buf_len, size_t *out_got)
{
	FILE *f;
	struct zip_member member;
	size_t want;
	size_t got = 0;

	if (out_got)
		*out_got = 0;
	if (!zip_path || !buf || buf_len == 0)
		return -1;

	f = fopen(zip_path, "rb");
	if (!f)
		return -1;

	if (zip_locate_rom_member(f, &member) != 0 ||
	    zip_seek_member_data(f, &member) != 0) {
		fclose(f);
		return -1;
	}

	want = buf_len;
	if (want > member.uncomp_size)
		want = member.uncomp_size;

	if (member.method == ZIP_METHOD_STORE) {
		got = fread(buf, 1, want, f);
		if (got != want) {
			fclose(f);
			return -1;
		}
	} else if (zip_inflate_into(f, member.comp_size, buf, want, &got) != 0) {
		fclose(f);
		return -1;
	}

	fclose(f);
	if (out_got)
		*out_got = got;
	return 0;
}

int zip_rom_extract(const char *zip_path, uint8_t **out_data, size_t *out_size)
{
	FILE *f;
	struct zip_member member;
	uint8_t *data;
	size_t got = 0;

	if (out_data)
		*out_data = NULL;
	if (out_size)
		*out_size = 0;
	if (!zip_path || !out_data || !out_size)
		return -1;

	f = fopen(zip_path, "rb");
	if (!f)
		return -1;

	if (zip_locate_rom_member(f, &member) != 0 ||
	    zip_seek_member_data(f, &member) != 0) {
		fclose(f);
		return -1;
	}

	data = (uint8_t *)malloc(member.uncomp_size);
	if (!data) {
		fclose(f);
		return -1;
	}

	if (member.method == ZIP_METHOD_STORE) {
		got = fread(data, 1, member.uncomp_size, f);
		if (got != member.uncomp_size) {
			free(data);
			fclose(f);
			return -1;
		}
	} else if (zip_inflate_into(f, member.comp_size, data, member.uncomp_size,
				    &got) != 0 ||
		   got != member.uncomp_size) {
		free(data);
		fclose(f);
		return -1;
	}

	fclose(f);
	*out_data = data;
	*out_size = member.uncomp_size;
	return 0;
}
