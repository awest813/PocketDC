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

#ifndef ZIP_ROM_H
#define ZIP_ROM_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define ZIP_ROM_MAX_SIZE (8u * 1024u * 1024u)

bool zip_rom_path_is_zip(const char *path);
bool zip_rom_name_is_gb(const char *name);

/* Extract the first .gb/.gbc member. Caller frees *out_data on success. */
int zip_rom_extract(const char *zip_path, uint8_t **out_data, size_t *out_size);

/* Read a prefix of the first .gb/.gbc member (for cartridge headers). */
int zip_rom_read(const char *zip_path, uint8_t *buf, size_t buf_len, size_t *out_got);

#endif /* ZIP_ROM_H */
