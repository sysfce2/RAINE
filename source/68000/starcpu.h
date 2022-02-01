/*
** Starscream 680x0 emulation library
** Copyright 1997, 1998, 1999 Neill Corlett
**
** Refer to STARDOC.TXT for terms of use, API reference, and directions on
** how to compile.
*/

#ifndef __STARCPU_H__
#define __STARCPU_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Remember to byte-swap these regions. (read STARDOC.TXT for details) */
struct STARSCREAM_PROGRAMREGION {
	unsigned lowaddr;
	unsigned highaddr;
#if USE_MUSASHI == 2
	UINT8 *offset;
#else
	unsigned offset;
#endif
};

struct STARSCREAM_DATAREGION {
	unsigned lowaddr;
	unsigned highaddr;
	void    *memorycall;
	void    *userdata;
};

/* Memory structures for 16-bit data path */
#define STARSCREAM_CONTEXTINFO_MEM16                          \
	struct STARSCREAM_PROGRAMREGION *fetch;               \
	struct STARSCREAM_DATAREGION    *readbyte;            \
	struct STARSCREAM_DATAREGION    *readword;            \
	struct STARSCREAM_DATAREGION    *writebyte;           \
	struct STARSCREAM_DATAREGION    *writeword;           \
	struct STARSCREAM_PROGRAMREGION *s_fetch;             \
	struct STARSCREAM_DATAREGION    *s_readbyte;          \
	struct STARSCREAM_DATAREGION    *s_readword;          \
	struct STARSCREAM_DATAREGION    *s_writebyte;         \
	struct STARSCREAM_DATAREGION    *s_writeword;         \
	struct STARSCREAM_PROGRAMREGION *u_fetch;             \
	struct STARSCREAM_DATAREGION    *u_readbyte;          \
	struct STARSCREAM_DATAREGION    *u_readword;          \
	struct STARSCREAM_DATAREGION    *u_writebyte;         \
	struct STARSCREAM_DATAREGION    *u_writeword;         \

/* Memory structures for 16-bit data path with function code support */
#define STARSCREAM_CONTEXTINFO_MEM16FC                        \
	unsigned     (*f_readbyte) (unsigned f, unsigned a);  \
	unsigned     (*f_readword) (unsigned f, unsigned a);  \
	unsigned     (*f_writebyte)(unsigned f, unsigned a);  \
	unsigned     (*f_writeword)(unsigned f, unsigned a);  \

/* Memory structures for 32-bit sizable data path */
#define STARSCREAM_CONTEXTINFO_MEM32                          \
	struct STARSCREAM_PROGRAMREGION *fetch;               \
	struct STARSCREAM_DATAREGION    *readbus;             \
	struct STARSCREAM_DATAREGION    *writebus;            \
	struct STARSCREAM_PROGRAMREGION *s_fetch;             \
	struct STARSCREAM_DATAREGION    *s_readbus;           \
	struct STARSCREAM_DATAREGION    *s_writebus;          \
	struct STARSCREAM_PROGRAMREGION *u_fetch;             \
	struct STARSCREAM_DATAREGION    *u_readbus;           \
	struct STARSCREAM_DATAREGION    *u_writebus;          \
	unsigned     (*f_readbus) (unsigned f, unsigned a);   \
	unsigned     (*f_writebus)(unsigned f, unsigned a);   \

/* Common context info for all 680x0 types */
#define STARSCREAM_CONTEXTINFO_COMMON                         \
	void         (*resethandler)(void);                   \
	unsigned       dreg[8];                               \
	unsigned       areg[8];                               \
	unsigned       asp;                                   \
	unsigned       pc;                                    \
	unsigned       odometer;                              \
	unsigned char  interrupts[8];                         \
	unsigned short sr;                                    \

/* 68000-specific context info */
#define STARSCREAM_CONTEXTINFO_68000SPECIFIC                  \
	unsigned short contextfiller00;                       \

/* Context info for 68010 and higher */
#define STARSCREAM_CONTEXTINFO_68010                          \
	unsigned char  sfc;                                   \
	unsigned char  dfc;                                   \
	unsigned       vbr;                                   \
	void         (*bkpthandler)(void);                    \

/* 68010-specific context info */
#define STARSCREAM_CONTEXTINFO_68010SPECIFIC                  \
	unsigned char  loopmode;                              \
	unsigned char  contextfiller10[3];                    \

/* Context info for 68020 and higher */
#define STARSCREAM_CONTEXTINFO_68020                          \
	unsigned       asp2;                                  \

struct S68000CONTEXT {
	STARSCREAM_CONTEXTINFO_MEM16
	STARSCREAM_CONTEXTINFO_COMMON
	STARSCREAM_CONTEXTINFO_68000SPECIFIC
};

struct S68010CONTEXT {
	STARSCREAM_CONTEXTINFO_MEM16
	// STARSCREAM_CONTEXTINFO_MEM16FC // doesn't seem to be used at all !!!
	STARSCREAM_CONTEXTINFO_COMMON
	STARSCREAM_CONTEXTINFO_68010
	STARSCREAM_CONTEXTINFO_68010SPECIFIC
};

struct S68020CONTEXT {
	STARSCREAM_CONTEXTINFO_MEM32
	STARSCREAM_CONTEXTINFO_COMMON
	STARSCREAM_CONTEXTINFO_68010
	STARSCREAM_CONTEXTINFO_68020
};

#if USE_MUSASHI < 2
#define STARSCREAM_IDENTIFIERS(SNC,SN)                        \
                                                              \
extern struct SNC##CONTEXT SN##context;                       \
                                                              \
int      SN##init             (void);                         \
unsigned SN##reset            (void);                         \
unsigned SN##exec             (int n);                        \
int      SN##interrupt        (int level, int vector);        \
void     SN##flushInterrupts  (void);                         \
int      SN##GetContextSize   (void);                         \
void     SN##GetContext       (void *context);                \
void     SN##SetContext       (void *context);                \
int      SN##fetch            (unsigned address);             \
unsigned SN##readOdometer     (void);                         \
unsigned SN##tripOdometer     (void);                         \
unsigned SN##controlOdometer  (int n);                        \
void     SN##releaseTimeslice (void);                         \
unsigned SN##readPC           (void);                         \

STARSCREAM_IDENTIFIERS(S68000,s68000)
STARSCREAM_IDENTIFIERS(S68010,s68010)
// STARSCREAM_IDENTIFIERS(S68020,s68020)
#define s68000_sr (M68010Engine ? s68010context.sr : s68000context.sr)
#define s68000_interrupts s68000context.interrupts[0]
#define s68000_pc s68000context.pc
#define s68000_read_pc (M68010Engine ? s68010readPC() : s68000readPC())
#define s68000_areg (M68010Engine ? s68010context.areg : s68000context.areg)
#define s68000_dreg (M68010Engine ? s68010context.dreg : s68000context.dreg)
#define s68000_cycles_run s68000context.odometer
#else
#include "Musashi/m68kcpu.h"

#define s68000readOdometer m68k_cycles_run
#define s68000_sr m68ki_get_sr()
#define s68000_interrupts m68ki_cpu.int_level
#define s68000_pc REG_PC
#define s68000readPC() REG_PC
#define s68000_read_pc REG_PC
#define s68000_areg REG_A
#define s68000_dreg REG_D
#define s68000releaseTimeslice m68k_end_timeslice
#define s68000_cycles_run m68k_cycles_run()
#define s68000init()
#define s68000GetContext(ctx) m68k_get_context(ctx)
#define s68000SetContext(ctx) m68k_set_context(ctx)

#endif

#ifdef __cplusplus
}
#endif

#endif
