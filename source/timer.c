#include <raine.h>
#include <stdio.h>
#include "profile.h"
#include "cpumain.h"
#include "mz80.h"
#include "2151intf.h"
#include "mz80help.h"
#include "sasound.h"
#include "gui.h" // goto_debuger
#include "savegame.h"
#include "galaxian.h"
#include "pokey.h"

// Number of cycles before reseting the cycles counter and the timers
#define MAX_CYCLES 0x40000000

// Minimum number of cycles to execute between timers tigers
#define MIN_CYCLES 500
#define VERBOSE 0

/******************************************************************/
/* This is my attempt to fix music tempo in games using the sound */
/* chip to synchronize.                                           */
/*                                                                */
/* Description of the problem : */
/* The z80 is either using an irq generated by the sound chip or */
/* is simply waiting for the sound chip status to change. These */
/* sound chips are using timers to generate their irq or to adjust */
/* their status, and we can't just reproduce these timers since */
/* the z80 is taking about 5% of the cpu time of a frame (most */
/* of the irq would happen outside of the z80 frame...) */
/* */
/* Solution : I declare my timers as static structs containing the */
/* handler and the number of cycles to be executed by the z80 before */
/* the timer is trigered. */
/* Then a special z80 frame must be executed where only the required */
/* cycles to triger a timer are executed. This slows a little down the */
/* emulation (because the z80 frame is sliced), but it's more acurate */

typedef struct {
  void (*handler)(int);
  int param;
  UINT32 cycles;
  UINT32 id;
  UINT32 period;
  char name[16];
  int active;
} TimerStruct;

UINT32 audio_cpu;

#define MAX_TIMERS 20

static TimerStruct timer[MAX_TIMERS];
static int free_timer = 0; // index of first availale timer
static size_t timer_id = 1;   // id of next allocated timer
static int z80_frame;

void z80_irq_handler(int irq) {
  // printf("z80_irq_handler %d\n",irq);
#ifdef MAME_Z80
    // A single line, as it's supposed to be finally !
    // Notice that when using the other code with this emulator, aodk has no sound
    // I am not totally sure of the reason, maybe the cycles I execute after the default irq
    // are too much here ?
    switch_cpu(audio_cpu);
    z80_set_irq_line(0x38,INPUT_LINE_IRQ0,irq);
#else
  if (irq) {
    cpu_interrupt(audio_cpu, 0x38);
  }
#if 1
  else  {
    // When irq=0, normally the irq line goes low.
    // It seems usefull at least for rainbow Island. I should investigate
    // a little more about it one day...
    mz80ReleaseIRQ(audio_cpu & 0xf);
  }
#endif
#endif
}

void setup_z80_frame(UINT32 cpu, UINT32 cycles) {
  audio_cpu = cpu;
  z80_frame = cycles;
  timer_id = 1;
}

extern UINT32 pc_timer;

void reset_timers() {
  free_timer = 0;
  timer_id = 1;
  mz80ClearTimers();
  render_frame_count = pc_timer = cpu_frame_count = 0;
  reset_ingame_timer();
}

double emu_get_time() {
    // returns the time for the running cpu in seocnds (double)

    double time =  (double)cpu_get_cycles_done(audio_cpu)/(z80_frame*fps);
    return time;
}

double trunc(double);

double pos_in_frame() {
  // Returns the position in the current frame in % (0 - 1)
  /* This code is unprecise since it should verify which cpu is accessing the
     audio ! (it's supposed to be called only by the audio emulation */
  double pos = 1.0-(mz80GetCyclesRemaining()*1.0)/z80_frame;
  // printf("cycles : %d/%d pos %g\n",cyclesRemaining,z80_frame,pos);
  return pos;
}

static int called_adjust;

extern void timer_callback_2203(int param);
extern void timer_callback_2610(int param);
extern void cb_3812a (int chip);
extern void cb_3812b (int chip);
extern void timer_callback_a (int n);
extern void timer_callback_b (int n);
extern void timer_callback_b (int n);
extern void ymf278b_timer_a_tick(int num);
extern void ymf278b_timer_b_tick(int num);
extern void ymf278b_timer_busy_clear(int num);
extern void ymf278b_timer_ld_clear(int num);
extern void timer_callback_3812(int param);

void *timer_adjust(double duration, int param, double period, void (*callback)(int))
{
  UINT32 remaining = duration * fps * z80_frame;
  UINT32 cycles_period = period * fps * z80_frame;
  UINT32 elapsed = cpu_get_cycles_done(audio_cpu);

  called_adjust = 1;
  if (free_timer < MAX_TIMERS) {
#if VERBOSE
      printf("timer_set %g cycles %d fps %g z80_frame %d cyclesremaining %d id %d param %d\n",duration,remaining,fps,z80_frame,cyclesRemaining,timer_id,param);
#endif
    timer[free_timer].handler = callback;
    timer[free_timer].param = param;
    timer[free_timer].cycles = elapsed + remaining;
    timer[free_timer].id = timer_id;
    timer[free_timer].period = cycles_period;
    timer[free_timer].active = 1;
    if (callback == &timer_callback_2203) strcpy(timer[free_timer].name,"2203");
    else if (callback == &timer_callback_2610) strcpy(timer[free_timer].name, "2610");
    else if (callback == &cb_3812a) strcpy(timer[free_timer].name, "3812a");
    else if (callback == &cb_3812b) strcpy(timer[free_timer].name, "3812b");
    else if (callback == &timer_callback_a) strcpy(timer[free_timer].name, "2151a");
    else if (callback == &timer_callback_b) strcpy(timer[free_timer].name, "2151b");
    else if (callback == &ymf278b_timer_a_tick) strcpy(timer[free_timer].name, "ymf278b1");
    else if (callback == &ymf278b_timer_b_tick) strcpy(timer[free_timer].name, "ymf278b2");
    // else if (callback == &ymf278b_timer_busy_clear) strcpy(timer[free_timer].name, "ymf278b3");
    // else if (callback == &ymf278b_timer_ld_clear) strcpy(timer[free_timer].name, "ymf278b4");
    else if (callback == &timer_callback_3812) strcpy(timer[free_timer].name,"3812");
    else if (callback == &galaxian_noise_timer_cb) strcpy(timer[free_timer].name,"galax_n");
    else if (callback == &galaxian_lfo_timer_cb) strcpy(timer[free_timer].name,"galax_lfo");
    else if (callback == &pokey_timer_expire) strcpy(timer[free_timer].name,"pokey1");
    else if (callback == &pokey_pot_trigger) strcpy(timer[free_timer].name,"pokey2");
    else {
	fatal_error("timer_set: handler unknown");
    }
    free_timer++;
  } else {
    printf("free timers overflow !!!\n");
    return NULL;
    // exit(1);
  }
  return (void*)timer_id++;
}

static void restore_timers() {
    printf("restore timers %d\n",free_timer);
    for (int n=0; n<free_timer; n++) {
	if (!strcmp(timer[n].name,"ymf278b1")) timer[n].handler = &ymf278b_timer_a_tick;
	else if (!strcmp(timer[n].name,"ymf278b2")) timer[n].handler = &ymf278b_timer_b_tick;
	else if (!strcmp(timer[n].name,"2203")) timer[n].handler = &timer_callback_2203;
	else if (!strcmp(timer[n].name,"2610")) timer[n].handler = &timer_callback_2610;
	else if (!strcmp(timer[n].name,"3812a")) timer[n].handler = &cb_3812a;
	else if (!strcmp(timer[n].name,"3812b")) timer[n].handler = &cb_3812b;
	else if (!strcmp(timer[n].name,"2151a")) timer[n].handler = &timer_callback_a;
	else if (!strcmp(timer[n].name,"2151b")) timer[n].handler = &timer_callback_b;
	else if (!strcmp(timer[n].name,"3812")) timer[n].handler = &timer_callback_3812;
	else if (!strcmp(timer[n].name,"galax_n")) timer[n].handler = &galaxian_noise_timer_cb;
	else if (!strcmp(timer[n].name,"galax_lfo")) timer[n].handler = &galaxian_lfo_timer_cb;
	else if (!strcmp(timer[n].name,"pokey1")) timer[n].handler = &pokey_timer_expire;
	else if (!strcmp(timer[n].name,"pokey2")) timer[n].handler = &pokey_pot_trigger;
    }
}

void save_timers() {
    // Add the relevant data to save timers, must be called by those who need timers, not automatic
    AddSaveData_ext("timers",((UINT8*)&timer),sizeof(timer));
    AddSaveData_ext("free_timer",((UINT8*)&free_timer),sizeof(int));
    AddSaveData_ext("timer_id",((UINT8*)&timer_id),sizeof(size_t));
    AddLoadCallback(&restore_timers);
    AddSaveData_ext("cpu_frame_count",(UINT8*)&cpu_frame_count,sizeof(UINT32));
}

void timer_remove(void *the_timer) {
  int n;
  size_t id = (size_t)the_timer;
#if VERBOSE
  printf("CALLED timer_remove %d\n",id);
#endif
  for (n=0; n<free_timer; n++) {
    if (timer[n].id == id)
      break;
  }
  if (n<free_timer) { // found ?
    if (n<free_timer-1) // Not the last one ?
      memmove(&timer[n],&timer[n+1],sizeof(TimerStruct)*(free_timer-n-1));
    free_timer--;
  } else {
#ifdef RAINE_DEBUG
    // Not exactly sure how it happens, but it happens sometimes...
    printf("Timer not found %zd!\n",id);
#endif
    // exit(1);
  }
#if VERBOSE
  printf("after removal free %d\n",free_timer);
#endif
}

void triger_timers() {
  UINT32 elapsed = cpu_get_cycles_done(audio_cpu);
  int n;
#if VERBOSE
  int count=0;
#endif
#ifndef RAINE_DOS
  if (goto_debuger)
      return;
#endif
  /* cyclesRemaining is not reseted by mz80 at the end of its frame... */
  /* If we are here, the frame is over. */
  cyclesRemaining=0;

#if VERBOSE
  printf("elapsed %d free_timer %d\n",elapsed,free_timer);
#endif

  for (n=0; n<free_timer; n++) {
    if (timer[n].cycles <= elapsed && timer[n].active) { // Trigered !
#if VERBOSE
      printf("timer %d elapsed %d diff %d\n",n,elapsed,elapsed - timer[n].cycles);
      count++;
#endif
      // if here, not while. silentd messes its timers at start and recovers
      // after that, if there is a while here, it's an infinite loop
      // (the logic of the driver might be wrong, but it works this way anyway)
      // Only for mz80, to be tested in cz80 !
      if ((audio_cpu >> 4) == CPU_Z80 && MZ80Engine) {
	  // Don't know yet how this will work with other cpus, I expect trouble !
	  if (!_z80iff) {
	      // Sometimes 2 timers trigger too close to each other and the z80
	      // needs time to handle the interrupt.
	      // I really wonder how the original hardware handled this.
	      // Maybe there was a minimum delay on the line ?

	      // Anyway this has to happen BEFORE the timer is trigered because
	      // if an external interrupt blocks the timer int, even if it makes
	      // it pending, it can produce 2 interrupts too close to each other
	      // which has the effect of completely stopping the music !
	      ExitOnEI = 1; // Exit at the end of the interrupt
	      cpu_execute_cycles(audio_cpu, 240000 );
	      // printf("%d cycles more\n",dwElapsedTicks - elapsed);
	      ExitOnEI = 0;
	      dwElapsedTicks = elapsed; // This frame must not count for the timers
	      cyclesRemaining=0;
	  }
      }
      called_adjust = 0;
      (*(timer[n].handler))(timer[n].param);
      // Normally, I would call timer_remove here :
      // timer_remove((void*)timer[n].id);
      // but it is rather unefficient since it's looking for a timer
      // when we know which one we want to delete...
      // Also, it allows to handle periodic timers
      if (!called_adjust) {
	  if (timer[n].period) {
	      timer[n].cycles = elapsed + timer[n].period;
	  } else {
	      if (n<free_timer-1) // not the last timer ?
		  memmove(&timer[n],&timer[n+1],sizeof(TimerStruct)*(free_timer-n-1));
	      free_timer--;
	      n--;
	  }
      }
    }
  }
  if (elapsed > MAX_CYCLES) { // time to reset the cpu...
    for (n=0; n<free_timer; n++)
      timer[n].cycles -= MAX_CYCLES;
    cpu_set_cycles_done(audio_cpu,-MAX_CYCLES);
  }
#if VERBOSE
  if (count > 1)
    printf("*** WARNING triger %d\n",count);
#endif
}

INT32 get_min_cycles(UINT32 frame) {
  int n;
  UINT32 elapsed;
  INT32 min_cycles;

  elapsed = cpu_get_cycles_done(audio_cpu);
  if (free_timer > 0) {
    min_cycles = timer[0].cycles;
    for (n=1; n<free_timer; n++)
      if (timer[n].cycles < min_cycles)
	min_cycles = timer[n].cycles;
    min_cycles -= elapsed;

    if (min_cycles <= MIN_CYCLES) {
      min_cycles = MIN_CYCLES;
    }
    if (min_cycles > frame) {
      min_cycles = frame;
    }
  } else {
    min_cycles = frame;
  }
#if VERBOSE
  printf("asked %d\n",min_cycles);
  if (min_cycles < 0) {
    printf("cycles FIXED to 1000\n");
    min_cycles = 500;
    exit(1);
  }
#endif
  return min_cycles;
}

int execute_one_z80_audio_frame(UINT32 frame) {
#ifndef RAINE_DOS
  if (goto_debuger)
      return 0;
#endif
  if (RaineSoundCard) {
    UINT32 elapsed = cpu_get_cycles_done(audio_cpu);
    INT32 min_cycles = get_min_cycles(frame);

    cpu_execute_cycles(audio_cpu, min_cycles );        // Sound Z80
    frame = (cpu_get_cycles_done(audio_cpu) - elapsed); // min_cycles;
#if VERBOSE
    if (abs(frame - min_cycles) > 16)
      printf("diff %d (pc %x)\n",cpu_get_cycles_done(audio_cpu) - elapsed - min_cycles,cpu_get_pc(audio_cpu));
#endif

    triger_timers();
    //printf("%d\n",frame-min_cycles);
  }
  return frame;
}

void finish_speed_hack(INT32 diff) {
  INT32 min;
  while (diff > 0) {
    min = get_min_cycles(diff);
    cpu_set_cycles_done(audio_cpu,min);
    triger_timers();
    diff -= min;
  }
}

void execute_z80_audio_frame() {
  INT32 frame = z80_frame;
#ifndef RAINE_DOS
  if (goto_debuger)
      return;
#endif
  switch_cpu(audio_cpu);
  while (frame > 0) {
    frame -= execute_one_z80_audio_frame(frame);
  }
}

void timer_enable(void *t, int active) {
    int n;
    UINT32 id = (UINT32)t;
    for (n=0; n<free_timer; n++) {
	if (timer[n].id == id) {
	    timer[n].active = active;
	    break;
	}
    }
}


// For now this thing always runs the 68k at 4 times the speed of the z80.
void execute_z80_audio_frame_with_nmi(int nb) {
  INT32 frame = z80_frame;
  int step = frame/nb;
  while (frame > 0) {
    int cycles = get_min_cycles(frame);
    int n;
    if (step < cycles) {
      for (n=0; n<cycles; n+=step) {
	cpu_execute_cycles(audio_cpu,step);
	cpu_execute_cycles(CPU_68K_0, step*4); // Main 68000
	cpu_int_nmi(audio_cpu);
      }
    } else {
      cpu_execute_cycles(audio_cpu,cycles);
      cpu_execute_cycles(CPU_68K_0, cycles*4); // Main 68000
    }
    triger_timers();
    if (step > cycles) {
      cpu_execute_cycles(audio_cpu,step-cycles);
      cpu_execute_cycles(CPU_68K_0, (step-cycles)*4); // Main 68000
      cpu_int_nmi(audio_cpu);
    }
    frame -= cycles;
  }
}
