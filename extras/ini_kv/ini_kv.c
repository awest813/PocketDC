/*
 * PocketDC / Walnut-CGB — minimal key=value config helpers.
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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "ini_kv.h"

static const char *ini_kv_skip_space(const char *p)
{
	while (p && *p && isspace((unsigned char)*p))
		p++;

	return p;
}

static bool ini_kv_key_match(const char *line, const char *key, const char **value_out)
{
	size_t key_len;
	const char *p;

	if (!line || !key || !value_out)
		return false;

	key_len = strlen(key);
	p = ini_kv_skip_space(line);
	if (strncmp(p, key, key_len) != 0)
		return false;

	p += key_len;
	/* Reject prefix matches such as "volume" vs "volume_max". */
	if (*p != '\0' && *p != '=' && !isspace((unsigned char)*p))
		return false;

	p = ini_kv_skip_space(p);
	if (*p != '=')
		return false;

	*value_out = ini_kv_skip_space(p + 1);
	return true;
}

bool ini_kv_parse_bool(const char *value, bool *out)
{
	if (!value || !out)
		return false;

	if (strcmp(value, "1") == 0 || strcasecmp(value, "true") == 0 ||
	    strcasecmp(value, "yes") == 0) {
		*out = true;
		return true;
	}

	if (strcmp(value, "0") == 0 || strcasecmp(value, "false") == 0 ||
	    strcasecmp(value, "no") == 0) {
		*out = false;
		return true;
	}

	return false;
}

bool ini_kv_get_int(const char *line, const char *key, int *out)
{
	const char *value;
	char *end;

	if (!out || !ini_kv_key_match(line, key, &value))
		return false;

	*out = (int)strtol(value, &end, 10);
	return end != value;
}

bool ini_kv_get_string(const char *line, const char *key, char *out, size_t out_len)
{
	const char *value;
	size_t len;

	if (!out || out_len == 0 || !ini_kv_key_match(line, key, &value))
		return false;

	len = strlen(value);
	if (len >= out_len)
		len = out_len - 1;

	memcpy(out, value, len);
	out[len] = '\0';

	while (len > 0 && isspace((unsigned char)out[len - 1])) {
		out[len - 1] = '\0';
		len--;
	}

	return true;
}

void ini_kv_fprint_int(FILE *f, const char *key, int value)
{
	if (!f || !key)
		return;

	fprintf(f, "%s=%d\n", key, value);
}

void ini_kv_fprint_bool(FILE *f, const char *key, bool value)
{
	if (!f || !key)
		return;

	fprintf(f, "%s=%d\n", key, value ? 1 : 0);
}

void ini_kv_fprint_string(FILE *f, const char *key, const char *value)
{
	if (!f || !key)
		return;

	fprintf(f, "%s=%s\n", key, value ? value : "");
}
