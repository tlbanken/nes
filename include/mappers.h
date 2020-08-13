/*
 * mappers.h
 *
 * Travis Banken
 * 2020
 *
 * Header file for all the mappers.
 */

#ifndef _MAPPERS_H
#define _MAPPERS_H

#include <utils.h>

typedef bool (*mapper_rfunc_t)(u32*);
typedef bool (*mapper_wfunc_t)(u8, u32*);
typedef void (*mapper_init_t)(u8, u8);

// Mapper 0
void Map000_Init(u8 prgrom_banks, u8 chrrom_banks);
bool Map000_CpuRead(u32 *addr);
bool Map000_CpuWrite(u8 data, u32 *addr);
bool Map000_PpuRead(u32 *addr);
bool Map000_PpuWrite(u8 data, u32 *addr);

// Mapper 1
// void Map001_Init(u8 prgrom_banks, u8 chrrom_banks);
// bool Map001_CpuRead(u16 *addr);
// bool Map001_CpuWrite(u16 *addr);
// bool Map001_PpuRead(u16 *addr);
// bool Map001_PpuWrite(u16 *addr);

// Mapper 2
void Map002_Init(u8 prgrom_banks, u8 chrrom_banks);
bool Map002_CpuRead(u32 *addr);
bool Map002_CpuWrite(u8 data, u32 *addr);
bool Map002_PpuRead(u32 *addr);
bool Map002_PpuWrite(u8 data, u32 *addr);

#endif