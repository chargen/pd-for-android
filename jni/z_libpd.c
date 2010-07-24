/*
 * Copyright (c) 2010 Peter Brinkmann (peter.brinkmann@gmail.com)
 *
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution.
 */

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include "z_libpd.h"
#include "x_libpdreceive.h"
#include "s_stuff.h"

void pd_init();
int sys_startgui(const char *guipath);  // do we really need this?

t_libpd_printhook libpd_printhook = NULL;
t_libpd_banghook libpd_banghook = NULL;
t_libpd_floathook libpd_floathook = NULL;
t_libpd_symbolhook libpd_symbolhook = NULL;
t_libpd_listhook libpd_listhook = NULL;
t_libpd_messagehook libpd_messagehook = NULL;

static int ticks_per_buffer;

static void *get_object(const char *s) {
  t_pd *x = gensym(s)->s_thing;
  return x;
}

/* this is called instead of sys_main() to start things */
void libpd_init() {
  // are all these settings necessary?
  sys_printhook = (t_printhook) libpd_printhook;
  sys_externalschedlib = 0;
  sys_schedblocksize = DEFDACBLKSIZE;
  sys_printtostderr = 0;
  sys_usestdpath = 1;
  sys_debuglevel = 0;
  sys_verbose = 0;
  sys_noloadbang = 0;
  sys_nogui = 1;
  sys_hipriority = 0;
  sys_nmidiin = 0;
  sys_nmidiout = 0;
  sys_time = 0;
  pd_init();
  libpdreceive_setup();
  sys_set_audio_api(API_DUMMY);
  sys_startgui(NULL);
}

void libpd_setextrapath(const char *s) {
  sys_setextrapath(s);
}

int libpd_init_audio(int inChans, int outChans, int sampleRate, int tpb) {
  ticks_per_buffer = tpb;
  int indev[MAXAUDIOINDEV], inch[MAXAUDIOINDEV],
       outdev[MAXAUDIOOUTDEV], outch[MAXAUDIOOUTDEV];
  indev[0] = outdev[0] = DEFAULTAUDIODEV;
  inch[0] = inChans;
  outch[0] = outChans;
  sys_set_audio_settings(1, indev, 1, inch,
         1, outdev, 1, outch, sampleRate, -1, 1);
  sched_set_using_audio(SCHED_AUDIO_CALLBACK);
  sys_reopen_audio();
  return 0;
}

static const t_float float_to_short = SHRT_MAX,
                   short_to_float = 1.0 / (t_float) SHRT_MAX;

#define PROCESS(_x, _y) \
  int i, j, k; \
  t_float *p0, *p1; \
  for (i = 0; i < ticks_per_buffer; i++) { \
    for (j = 0, p0 = sys_soundin; j < DEFDACBLKSIZE; j++, p0++) { \
      for (k = 0, p1 = p0; k < sys_inchannels; k++, p1 += DEFDACBLKSIZE) { \
        *p1 = *inBuffer++ _x; \
      } \
    } \
    memset(sys_soundout, 0, sys_outchannels*DEFDACBLKSIZE*sizeof(t_float)); \
    sched_tick(sys_time + sys_time_per_dsp_tick); \
    for (j = 0, p0 = sys_soundout; j < DEFDACBLKSIZE; j++, p0++) { \
      for (k = 0, p1 = p0; k < sys_outchannels; k++, p1 += DEFDACBLKSIZE) { \
        *outBuffer++ = *p1 _y; \
      } \
    } \
  } \
  return 0;

int libpd_process_short(short *inBuffer, short *outBuffer) {
  PROCESS(* short_to_float, * float_to_short)
}

int libpd_process_float(float *inBuffer, float *outBuffer) {
  PROCESS(,)
}

int libpd_process_double(double *inBuffer, double *outBuffer) {
  PROCESS(,)
}

static t_atom argv[MAXMSGLENGTH], *curr;
static int argc;

int libpd_start_message() {
  argc = 0;
  curr = argv;
  return MAXMSGLENGTH;
}

#define ADD_ARG(f) f(curr, x); curr++; argc++;

void libpd_add_float(float x) {
  ADD_ARG(SETFLOAT);
}

void libpd_add_symbol(const char *s) {
  t_symbol *x = gensym(s);
  ADD_ARG(SETSYMBOL);
}

int libpd_finish_list(const char *recv) {
  t_pd *dest = gensym(recv)->s_thing;
  if (dest == NULL) return -1;
  pd_list(dest, &s_list, argc, argv);
  return 0;
}

int libpd_finish_message(const char *recv, const char *msg) {
  t_pd *dest = gensym(recv)->s_thing;
  if (dest == NULL) return -1;
  t_symbol *sym = gensym(msg);
  pd_typedmess(dest, sym, argc, argv);
  return 0;
}

void *libpd_bind(const char *sym) {
  return libpdreceive_new(gensym(sym));
}

int libpd_unbind(void *p) {
  pd_free((t_pd *)p);
  return 0;
}

int libpd_symbol(const char *recv, const char *sym) {
  void *obj = get_object(recv);
  if (obj != NULL) {
    pd_symbol(obj, gensym(sym));
    return 0;
  } else {
    return -1;
  }
}

int libpd_float(const char *recv, float x) {
  void *obj = get_object(recv);
  if (obj != NULL) {
    pd_float(obj, x);
    return 0;
  } else {
    return -1;
  }
}

int libpd_bang(const char *recv) {
  void *obj = get_object(recv);
  if (obj != NULL) {
    pd_bang(obj);
    return 0;
  } else {
    return -1;
  }
}

int libpd_blocksize() {
  return DEFDACBLKSIZE;
}

int libpd_exists(const char *sym) {
  return get_object(sym) != NULL;
}

