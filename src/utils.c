/*
 * utils.c
 *
 * Travis Banken
 * 2020
 *
 * Utilities for the nes emulator.
 */

#include <stdlib.h>
#include <utils.h>

#define LMAP_SIZE 4
static FILE* lmap[LMAP_SIZE] = {0};

static bool log_on = false;

void neslog_init()
{
	log_on = true;
}

void neslog_cleanup()
{
	for (int id = 0; id < LMAP_SIZE; id++) {
		if (lmap[id] != NULL && lmap[id] != stdout && lmap[id] != stderr) {
			fclose(lmap[id]);
			lmap[id] = NULL;
		}
	}
}

void neslog_add(lid_t id, char *path)
{
	// default to stderr
	if (path == NULL) {
		lmap[id] = stderr;
		return;
	}

	FILE *file = fopen(path, "w");
	if (file == NULL) {
		perror("fopen");
		exit(1);
	}
	lmap[id] = file;
}

void neslog(lid_t id, const char *fmt, ...)
{
	if (!log_on) {
		return;
	}

	// make sure log id valid
	if (lmap[id] == NULL) {
		PERROR("Invalid log id (%d)\n", id);
		exit(1);
	}

	va_list args;
	va_start(args, fmt);
	vfprintf(lmap[id], fmt, args);
	va_end(args);
}