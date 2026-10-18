// Minimal stand-in for the KORG logue SDK header, so the DSP can be tested on a PC.
#pragma once
#include <stdint.h>
#include <math.h>
#include <stdlib.h>
typedef int32_t q31_t;
typedef struct { int32_t shape_lfo; uint16_t pitch; uint16_t cutoff, resonance; } user_osc_param_t;
enum { k_user_osc_param_id1=0,k_user_osc_param_id2,k_user_osc_param_id3,k_user_osc_param_id4,k_user_osc_param_id5,k_user_osc_param_id6,k_user_osc_param_shape,k_user_osc_param_shiftshape };
#define param_val_to_f32(val) ((uint16_t)val * 9.77517106549365e-004f)
#define q31_to_f32(q) ((float)(q) / 2147483648.f)
#define f32_to_q31(f) ((q31_t)((f) * 2147483647.f))
#define OSC_INIT _hook_init
#define OSC_CYCLE _hook_cycle
#define OSC_NOTEON _hook_on
#define OSC_NOTEOFF _hook_off
#define OSC_PARAM _hook_param
static inline float clipminmaxf(float a,float x,float b){return x<a?a:(x>b?b:x);}
static inline float clipmaxf(float x,float m){return x>m?m:x;}
static inline float clip1m1f(float x){return clipminmaxf(-1,x,1);}
static inline float si_fabsf(float x){return fabsf(x);}
static inline float osc_sinf(float x){ float p=x-(uint32_t)x; return sinf(6.2831853f*p);}
static inline float osc_white(void){return 2.f*rand()/(float)RAND_MAX-1.f;}
static inline uint32_t osc_rand(void){return (uint32_t)rand()*2654435761u;}
static inline float osc_softclipf(float c,float x){x=clip1m1f(x);return x-c*x*x*x;}
static inline float osc_w0f_for_note(uint8_t n,uint8_t m){return 440.f*powf(2.f,(n+m/256.f-69)/12.f)/48000.f;}
