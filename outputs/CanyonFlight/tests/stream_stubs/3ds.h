#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
typedef uint64_t u64;
typedef void *Thread;typedef int LightEvent;
#define RESET_ONESHOT 0
#define U64_MAX UINT64_MAX
Thread threadCreate(void (*entry)(void*),void*,size_t,int,int,bool);
void threadJoin(Thread,u64);void threadFree(Thread);
void LightEvent_Init(LightEvent*,int);void LightEvent_Signal(LightEvent*);void LightEvent_Wait(LightEvent*);
void GSPGPU_FlushDataCache(void*,size_t);
typedef uint32_t u32;u32 linearSpaceFree(void);
typedef int64_t s64;void svcSleepThread(s64 ns); /* v35.06 retry sleep in scenery_stream.c */
