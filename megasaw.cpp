/*
 * MegaSaw XD : 2-layer JP-8000 style supersaw oscillator for KORG minilogue xd
 * Author: SpeakingColors
 *
 * A merge of HyperSaw (selectable waves per layer, octave, balance, Vox Ah)
 * and HyperSawX (more voices, analog drift). Each layer is a stack of 3 to 11
 * detuned voices using the JP-8000 detune and mix curves
 * (Adam Szabo, "How to Emulate the Super Saw", 2010).
 *
 * Controls
 *   SHAPE        : Detune (JP curve), also follows the xd's LFO / mod routing
 *   SHIFT+SHAPE  : Mix (center voice vs side voices, JP curve)
 *   Wave A / B   : 0 Off, 1 Saw, 2 Square, 3 Pulse 25%, 4 Pulse 12%,
 *                  5 Triangle, 6 Sine, 7 Glass 1, 8 Glass 2, 9 Vox Ah
 *   Oct B        : -2..+2 octaves relative to layer A
 *   Layer Bal    : -100 = layer A only, 0 = equal, +100 = layer B only
 *   Voices       : -2..+2 = 3, 5, 7, 9, 11 voices per layer (0 = 7, same as HyperSaw)
 *   Drift        : each voice slowly wanders in pitch, like analog
 */

#include "userosc.h"

#define NUM_LAYERS 2
#define MAX_VOICES 11

enum {
  W_OFF = 0, W_SAW, W_SQUARE, W_PULSE25, W_PULSE12,
  W_TRI, W_SINE, W_GLASS1, W_GLASS2, W_VOX, W_COUNT
};

// Relative detune offsets per voice count. Index 3 of the 7-voice table is the
// exact JP-8000 set; the others follow the same curve shape (outer voice = 0.11).
static const float k_off3[3]   = { -0.110000f, 0.f, 0.110000f };
static const float k_off5[5]   = { -0.110000f, -0.038891f, 0.f, 0.038891f, 0.110000f };
static const float k_off7[7]   = { -0.11002313f, -0.06288439f, -0.01952356f, 0.f,
                                    0.01991221f,  0.06216538f,  0.10745242f };
static const float k_off9[9]   = { -0.110000f, -0.071447f, -0.038891f, -0.013750f, 0.f,
                                    0.013750f,  0.038891f,  0.071447f,  0.110000f };
static const float k_off11[11] = { -0.110000f, -0.078710f, -0.051123f, -0.027828f, -0.009839f, 0.f,
                                    0.009839f,  0.027828f,  0.051123f,  0.078710f,  0.110000f };

// Output gain and per-wave level trims (all waves sit at a similar loudness)
static const float k_out_gain = 0.8f;
static const float k_wave_trim[W_COUNT] = {
  0.f,   // Off
  1.0f,  // Saw
  0.6f,  // Square
  0.6f,  // Pulse 25
  0.75f, // Pulse 12
  1.05f, // Triangle
  0.85f, // Sine
  0.7f,  // Glass 1
  0.95f, // Glass 2
  1.0f   // Vox Ah (final level set by k_vox_gain)
};

// "Ah" vowel formants: bandpass {b0 (incl. gain), a1, a2}; b1 = 0, b2 = -b0
#define NUM_FORMANTS 4
static const float k_formant[NUM_FORMANTS][3] = {
  { 0.00788823f, -1.97517136f, 0.98422353f }, // 730 Hz  (F1)
  { 0.00528542f, -1.96223771f, 0.98238195f }, // 1090 Hz (F2)
  { 0.00540993f, -1.86950050f, 0.96908609f }, // 2440 Hz (F3)
  { 0.00451083f, -1.77904561f, 0.95899244f }, // 3300 Hz (F4, "air")
};
static const float k_vox_gain    = 2.6f;    // overall Vox level
static const float k_vox_lp      = 0.1674f; // one-pole LPF ~1.4 kHz: softens the saw buzz
static const float k_vox_lp_comp = 2.2f;    // makes up level lost in the LPF
static const float k_vox_breath  = 0.6f;    // breath noise amount
static const float k_vox_drift   = 0.0035f; // Vox always drifts a little (~6 cents)
static const float k_vox_detune  = 1.6f;    // Vox spreads wider: formants mask the beating
static const float k_drift_max   = 0.004f;  // Drift knob at 100% (~7 cents)

struct State {
  float phase[NUM_LAYERS][MAX_VOICES];
  uint8_t wave[NUM_LAYERS];
  int8_t  oct_b;
  uint8_t nv;                 // voices per layer: 3, 5, 7, 9 or 11
  float shape;                // detune knob 0..1
  float shiftshape;           // mix knob 0..1
  float balance;              // -1..1
  float drift_amt;            // 0..1
  float hp_x1, hp_y1;
  float fz[NUM_LAYERS][NUM_FORMANTS][2];  // formant filter state per layer
  float vox_lp[NUM_LAYERS];               // source smoothing state
  float drift[NUM_LAYERS][MAX_VOICES];    // current pitch drift per voice
  float drift_t[NUM_LAYERS][MAX_VOICES];  // drift target
  uint32_t drift_tick;
};

static State s;

static inline float rand01(void) {
  return (float)(osc_rand() >> 8) * 5.96046447753906e-008f; // /2^24
}

// ---------------------------------------------------------------------------
// JP-8000 detune knob response (Szabo, 11th order polynomial fit)
static inline float jp_detune_curve(float x) {
  float y = 10028.7312891634f;
  y = y * x - 50818.8652045924f;
  y = y * x + 111363.4808729368f;
  y = y * x - 138150.6761080548f;
  y = y * x + 106649.6679158292f;
  y = y * x - 53046.9642751875f;
  y = y * x + 17019.9518580080f;
  y = y * x - 3425.0836591318f;
  y = y * x + 404.2703938388f;
  y = y * x - 24.1878824391f;
  y = y * x + 0.6717417634f;
  y = y * x + 0.0030115596f;
  return clipminmaxf(0.f, y, 1.f);
}

// Random-phase voices add up by power, not by amplitude, so more voices sound
// quieter. These factors (sqrt(nv/7)) keep every voice count at the same loudness.
static inline float level_comp_for(uint8_t nv) {
  switch (nv) {
  case 3:  return 0.6547f;
  case 5:  return 0.8452f;
  case 9:  return 1.1339f;
  case 11: return 1.2536f;
  default: return 1.f;
  }
}

static inline const float *offsets_for(uint8_t nv) {
  switch (nv) {
  case 3:  return k_off3;
  case 5:  return k_off5;
  case 9:  return k_off9;
  case 11: return k_off11;
  default: return k_off7;
  }
}

// ---------------------------------------------------------------------------
// Band-limited helpers (inv_dt precomputed to avoid divisions per sample)
static inline float poly_blep(float t, float dt, float inv_dt) {
  if (t < dt) {
    t *= inv_dt;
    return t + t - t * t - 1.f;
  } else if (t > 1.f - dt) {
    t = (t - 1.f) * inv_dt;
    return t * t + t + t + 1.f;
  }
  return 0.f;
}

static inline float saw_blep(float p, float dt, float inv_dt) {
  return (2.f * p - 1.f) - poly_blep(p, dt, inv_dt);
}

static inline float pulse_blep(float p, float dt, float inv_dt, float pw) {
  float q = p + pw;
  if (q >= 1.f) q -= 1.f;
  return saw_blep(p, dt, inv_dt) - saw_blep(q, dt, inv_dt);
}

// Renders one voice of one layer into acc[] (added). Wave switch is outside the sample loop.
static void render_voice(uint8_t w, float *phase, float dt, float g, float *acc, uint32_t frames) {
  float p = *phase;
  const float inv_dt = 1.f / dt;
  float pw = 0.5f;
  switch (w) {
  case W_SAW:
    for (uint32_t i = 0; i < frames; ++i) {
      acc[i] += g * saw_blep(p, dt, inv_dt);
      p += dt; if (p >= 1.f) p -= 1.f;
    }
    break;
  case W_PULSE25: pw = 0.25f;  goto pulse;
  case W_PULSE12: pw = 0.125f; goto pulse;
  case W_SQUARE:
  pulse:
    for (uint32_t i = 0; i < frames; ++i) {
      acc[i] += g * pulse_blep(p, dt, inv_dt, pw);
      p += dt; if (p >= 1.f) p -= 1.f;
    }
    break;
  case W_TRI:
    for (uint32_t i = 0; i < frames; ++i) {
      acc[i] += g * (2.f * si_fabsf(2.f * p - 1.f) - 1.f);
      p += dt; if (p >= 1.f) p -= 1.f;
    }
    break;
  case W_SINE:
    for (uint32_t i = 0; i < frames; ++i) {
      acc[i] += g * osc_sinf(p);
      p += dt; if (p >= 1.f) p -= 1.f;
    }
    break;
  case W_GLASS1: // sine phase-modulated by its 2nd harmonic: bright, glassy
    for (uint32_t i = 0; i < frames; ++i) {
      acc[i] += g * osc_sinf(p + 2.f + 0.12f * osc_sinf(2.f * p));
      p += dt; if (p >= 1.f) p -= 1.f;
    }
    break;
  case W_GLASS2: // 3rd harmonic PM + 5th harmonic shimmer, bell-like
    for (uint32_t i = 0; i < frames; ++i) {
      acc[i] += g * (0.8f * osc_sinf(p + 2.f + 0.18f * osc_sinf(3.f * p))
                   + 0.2f * osc_sinf(5.f * p));
      p += dt; if (p >= 1.f) p -= 1.f;
    }
    break;
  default: break;
  }
  *phase = p;
}

static void randomize_phases(void) {
  for (int l = 0; l < NUM_LAYERS; ++l)
    for (int v = 0; v < MAX_VOICES; ++v)
      s.phase[l][v] = rand01();
}

// ---------------------------------------------------------------------------
void OSC_INIT(uint32_t platform, uint32_t api) {
  (void)platform; (void)api;
  s.wave[0] = W_SAW; s.wave[1] = W_OFF;
  s.oct_b = 0;
  s.nv = 7;
  s.shape = 0.5f;
  s.shiftshape = 0.5f;
  s.balance = 0.f;
  s.drift_amt = 0.f;
  s.hp_x1 = s.hp_y1 = 0.f;
  for (int l = 0; l < NUM_LAYERS; ++l) {
    for (int k = 0; k < NUM_FORMANTS; ++k) s.fz[l][k][0] = s.fz[l][k][1] = 0.f;
    s.vox_lp[l] = 0.f;
    for (int v = 0; v < MAX_VOICES; ++v) s.drift[l][v] = s.drift_t[l][v] = 0.f;
  }
  s.drift_tick = 0;
  randomize_phases();
}

void OSC_CYCLE(const user_osc_param_t * const params, int32_t *yn, const uint32_t frames) {
  // Base pitch
  const float w0 = osc_w0f_for_note((params->pitch) >> 8, params->pitch & 0xFF);

  // Detune (knob + LFO), JP curve
  const float det_knob = clipminmaxf(0.f, s.shape + q31_to_f32(params->shape_lfo), 1.f);
  const float detune = jp_detune_curve(det_knob);

  const int nv = s.nv;
  const float *off = offsets_for(s.nv);
  const int center = nv >> 1;

  // Mix, JP curve: center voice vs the side voices
  const float m = s.shiftshape;
  const float g_center = -0.55366f * m + 0.99785f;
  const float g_side   = -0.73764f * m * m + 1.2841f * m + 0.044372f;
  const float norm = level_comp_for(s.nv) / (g_center + (float)(nv - 1) * g_side);

  // Active layers (if both are Off, layer A falls back to Saw)
  uint8_t wave[NUM_LAYERS] = { s.wave[0], s.wave[1] };
  if (wave[0] == W_OFF && wave[1] == W_OFF) wave[0] = W_SAW;

  // Per-layer gains from balance
  const float bal = s.balance;
  const float gA = (bal > 0.f) ? (1.f - bal) : 1.f;
  const float gB = (bal < 0.f) ? (1.f + bal) : 1.f;
  const float lgain[NUM_LAYERS] = { gA, gB };

  float active_sum = 0.f;
  for (int l = 0; l < NUM_LAYERS; ++l)
    if (wave[l] != W_OFF) active_sum += lgain[l];
  const float layer_norm = (active_sum > 0.f) ? (1.f / active_sum) : 0.f;

  // Drift: new random targets now and then (Vox always keeps a little)
  if ((s.drift_tick & 31) == 0) {
    for (int l = 0; l < NUM_LAYERS; ++l) {
      float amp = s.drift_amt * k_drift_max;
      if (wave[l] == W_VOX && amp < k_vox_drift) amp = k_vox_drift;
      for (int v = 0; v < nv; ++v)
        s.drift_t[l][v] = amp * (2.f * rand01() - 1.f);
    }
  }

  // Per-voice phase increments and gains
  float dt[NUM_LAYERS][MAX_VOICES];
  float vgain[NUM_LAYERS][MAX_VOICES];
  float lowest_w = 0.5f;
  for (int l = 0; l < NUM_LAYERS; ++l) {
    const int8_t o = (l == 1) ? s.oct_b : 0;
    const float wl = w0 * ((o >= 0) ? (float)(1 << o) : 1.f / (float)(1 << (-o)));
    if (wave[l] != W_OFF && wl < lowest_w) lowest_w = wl;
    const float det_l = (wave[l] == W_VOX) ? detune * k_vox_detune : detune;
    for (int v = 0; v < nv; ++v) {
      s.drift[l][v] += 0.02f * (s.drift_t[l][v] - s.drift[l][v]);
      dt[l][v] = clipmaxf(wl * (1.f + det_l * off[v] + s.drift[l][v]), 0.45f);
      const float g = (v == center) ? g_center : g_side;
      vgain[l][v] = g * norm * lgain[l] * layer_norm * k_wave_trim[wave[l]];
    }
  }

  // JP-style high-pass at the fundamental of the lowest active layer (one-pole)
  const float hp_a = 1.f / (1.f + 6.2831853f * lowest_w);

  float acc[64];  // SDK guarantees frames <= 64
  for (uint32_t i = 0; i < frames; ++i) acc[i] = 0.f;
  for (int l = 0; l < NUM_LAYERS; ++l) {
    if (wave[l] == W_OFF) continue;
    if (wave[l] == W_VOX) {
      float lbuf[64];
      for (uint32_t i = 0; i < frames; ++i) lbuf[i] = 0.f;
      for (int v = 0; v < nv; ++v)
        render_voice(W_SAW, &s.phase[l][v], dt[l][v], vgain[l][v], lbuf, frames);
      // Soften the buzzy saw source, then add breath noise before the vowel filters
      float lp = s.vox_lp[l];
      const float breath = k_vox_breath * lgain[l] * layer_norm;
      for (uint32_t i = 0; i < frames; ++i) {
        lp += k_vox_lp * (lbuf[i] - lp);
        lbuf[i] = lp * k_vox_lp_comp + breath * osc_white();
      }
      s.vox_lp[l] = lp;
      for (int k = 0; k < NUM_FORMANTS; ++k) {
        const float b0 = k_formant[k][0] * k_vox_gain, a1 = k_formant[k][1], a2 = k_formant[k][2];
        float z1 = s.fz[l][k][0], z2 = s.fz[l][k][1];
        for (uint32_t i = 0; i < frames; ++i) {   // transposed direct form II
          const float x = lbuf[i];
          const float yv = b0 * x + z1;
          z1 = -a1 * yv + z2;
          z2 = -b0 * x - a2 * yv;
          acc[i] += yv;
        }
        s.fz[l][k][0] = z1; s.fz[l][k][1] = z2;
      }
    } else {
      for (int v = 0; v < nv; ++v)
        render_voice(wave[l], &s.phase[l][v], dt[l][v], vgain[l][v], acc, frames);
    }
  }

  ++s.drift_tick;

  float hp_x1 = s.hp_x1, hp_y1 = s.hp_y1;
  q31_t * __restrict y = (q31_t *)yn;
  for (uint32_t i = 0; i < frames; ++i) {
    const float sig = acc[i];
    const float hp = hp_a * (hp_y1 + sig - hp_x1);   // one-pole HPF
    hp_x1 = sig;
    hp_y1 = hp;
    y[i] = f32_to_q31(osc_softclipf(0.05f, hp * k_out_gain));
  }

  s.hp_x1 = hp_x1;
  s.hp_y1 = hp_y1;
}

void OSC_NOTEON(const user_osc_param_t * const params) {
  (void)params;
  randomize_phases();   // JP-8000 free-running / random phase behaviour
}

void OSC_NOTEOFF(const user_osc_param_t * const params) {
  (void)params;
}

static inline int8_t clamp_pm2(int v) {
  return (int8_t)((v < -2) ? -2 : (v > 2) ? 2 : v);
}

void OSC_PARAM(uint16_t index, uint16_t value) {
  switch (index) {
  case k_user_osc_param_id1: s.wave[0] = (value < (uint16_t)W_COUNT) ? (uint8_t)value : (uint8_t)W_OFF; break;
  case k_user_osc_param_id2: s.wave[1] = (value < (uint16_t)W_COUNT) ? (uint8_t)value : (uint8_t)W_OFF; break;
  // bipolar % params arrive as 0..200 with 0 at 100
  case k_user_osc_param_id3: s.oct_b = clamp_pm2((int)value - 100); break;
  case k_user_osc_param_id4:
    s.balance = clipminmaxf(-1.f, ((int)value - 100) * 0.01f, 1.f);
    break;
  case k_user_osc_param_id5: s.nv = (uint8_t)(7 + 2 * clamp_pm2((int)value - 100)); break;
  case k_user_osc_param_id6: s.drift_amt = clipminmaxf(0.f, value * 0.01f, 1.f); break;
  case k_user_osc_param_shape:      s.shape = param_val_to_f32(value); break;
  case k_user_osc_param_shiftshape: s.shiftshape = param_val_to_f32(value); break;
  default: break;
  }
}
