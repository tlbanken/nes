/*
 * utils.h
 *
 * Travis Banken
 * 2020
 *
 * Header file for the nes utils.
 */

#ifndef _UTILS_H
#define _UTILS_H

#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>
#include <stdarg.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

// log ids
typedef enum lid {
    LID_CPU  = 0,
    LID_PPU  = 1,
    LID_APU  = 2,
    LID_CART = 3,
} lid_t;

void Neslog_Log(lid_t id, const char *fmt, ...);
void Neslog_Add(lid_t id, char *path);
void Neslog_Init();
void Neslog_Free();
void Utils_SetExitHandler(void (*func)(int));
void Utils_ExitWithHandler(int rc);
unsigned char Utils_FlipByte(unsigned char b);
char* op_to_str(u8 opcode);

// error codes
#define OK 0
#define ERR 1

#define ERROR(fmt, ...) \
    fprintf(stderr, "[%s] ERROR: ", __FUNCTION__); \
    fprintf(stderr, fmt, ##__VA_ARGS__);

#define WARNING(fmt, ...) \
    fprintf(stderr, "[%s] WARNING: ", __FUNCTION__); \
    fprintf(stderr, fmt, ##__VA_ARGS__);

#define INFO(fmt, ...) \
    fprintf(stderr, "[%s] INFO: ", __FUNCTION__); \
    fprintf(stderr, fmt, ##__VA_ARGS__);

#define EXIT(rc) Utils_ExitWithHandler(rc);


#endif