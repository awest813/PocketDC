OPT=-g2 -O2

override CFLAGS += $(OPT) -Wall -Wextra

EXTRAS_DIR := ../extras
INI_KV_SRC := $(EXTRAS_DIR)/ini_kv/ini_kv.c
AUDIO_PROCESSOR_SRC := $(EXTRAS_DIR)/audio_processor/audio_processor.c
AUDIO_RING_SRC := $(EXTRAS_DIR)/audio_ring/audio_ring.c
ZIP_ROM_SRC := $(EXTRAS_DIR)/zip_rom/zip_rom.c

.PHONY: all ci
all: extras_test test test_so
ci: extras_test
test: test.c walnut_cgb.o
	$(CC) $^ -o $@ -DPEANUT_GB_HEADER_ONLY $(CFLAGS)

test_so: test.c walnut_cgb.o
	$(CC) $^ -o $@ -DPEANUT_GB_HEADER_ONLY $(CFLAGS)

extras_test: extras_test.o ini_kv.o audio_processor.o audio_ring.o zip_rom.o
	$(CC) $^ -o $@ $(CFLAGS) -lm -lz

extras_test.o: extras_test.c
	$(CC) -c $< -o $@ $(CFLAGS) -I$(EXTRAS_DIR)/ini_kv -I$(EXTRAS_DIR)/audio_processor -I$(EXTRAS_DIR)/audio_ring -I$(EXTRAS_DIR)/zip_rom

ini_kv.o: $(INI_KV_SRC)
	$(CC) -c $< -o $@ $(CFLAGS) -I$(EXTRAS_DIR)/ini_kv

audio_processor.o: $(AUDIO_PROCESSOR_SRC)
	$(CC) -c $< -o $@ $(CFLAGS) -I$(EXTRAS_DIR)/audio_processor

audio_ring.o: $(AUDIO_RING_SRC)
	$(CC) -c $< -o $@ $(CFLAGS) -I$(EXTRAS_DIR)/audio_ring

zip_rom.o: $(ZIP_ROM_SRC)
	$(CC) -c $< -o $@ $(CFLAGS) -I$(EXTRAS_DIR)/zip_rom

test_external_rom: test_external_rom.c
	$(CC) $^ -o $@ $(CFLAGS)

walnut_cgb.o: ../walnut_cgb.h
	cp ../walnut_cgb.h ./walnut_cgb.c
	$(CC) -c walnut_cgb.c -o $@ $(CFLAGS)
	$(CC) -c walnut_cgb.c -S -o $@.S $(CFLAGS)

.PHONY: run run-extras
run: test extras_test
	./test
	./extras_test

run-extras: extras_test
	./extras_test
