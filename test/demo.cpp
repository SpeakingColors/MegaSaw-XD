// Renders a short demo of three MegaSaw presets (same riff each), mono 48 kHz, raw float output.
#include "../megasaw.cpp"
#include <stdio.h>
#include <vector>
struct Nt { int m, st, len; };
int main() {
  const float FS = 48000.f, bpm = 150, s16 = 60.f / bpm / 4.f;
  std::vector<Nt> ph = {{69,0,2},{69,3,1},{72,4,2},{76,6,2},{74,8,2},{72,10,1},{71,11,1},{72,12,3},
                        {77,16,2},{76,19,1},{74,20,2},{72,22,2},{71,24,2},{72,26,1},{74,27,1},{76,28,4}};
  struct P { const char *name; int a, b, ob, bal, vi, drift, shape, shift; } pats[] = {
    {"Classic JP lead",   1, 0, 100, 100, 100,  0, 614, 716},
    {"Mega stack",        1, 1,  99, 100, 102, 30, 716, 614},
    {"Hardstyle Ah lead", 9, 1, 100,  80, 101, 30, 716, 614}};
  std::vector<float> all; const float atk = 0.003f * FS, dec = 0.14f * FS, sus = 0.35f, rel = 0.11f * FS;
  for (auto &pt : pats) {
    OSC_INIT(0, 0); OSC_PARAM(0, pt.a); OSC_PARAM(1, pt.b); OSC_PARAM(2, pt.ob); OSC_PARAM(3, pt.bal);
    OSC_PARAM(4, pt.vi); OSC_PARAM(5, pt.drift); OSC_PARAM(6, pt.shape); OSC_PARAM(7, pt.shift);
    const int N = (int)(2 * 16 * s16 * FS) + (int)(FS * 0.8f); std::vector<float> out(N, 0.f);
    user_osc_param_t p = {0, 0, 0, 0}; int32_t buf[16]; size_t ni = 0; float env = 0, lpz = 0; int stage = 0; long offAt = -1;
    for (long t = 0; t < N; t += 16) {
      for (; ni < ph.size() && (long)(ph[ni].st * s16 * FS) <= t; ni++) { p.pitch = (uint16_t)((ph[ni].m - 12) << 8); OSC_NOTEON(&p); stage = 1; offAt = (long)((ph[ni].st + ph[ni].len) * s16 * FS - 0.01f * FS); }
      if (offAt >= 0 && t >= offAt) { stage = 4; offAt = -1; }
      OSC_CYCLE(&p, buf, 16);
      for (int i = 0; i < 16 && t + i < N; i++) {
        if (stage == 1) { env += 1.f / atk; if (env >= 1) { env = 1; stage = 2; } }
        else if (stage == 2) { env -= (1 - sus) / dec; if (env <= sus) { env = sus; stage = 3; } }
        else if (stage == 4) { env -= sus / rel; if (env <= 0) { env = 0; stage = 0; } }
        lpz += 0.55f * (buf[i] / 2147483648.f - lpz);   // gentle xd-style lowpass
        out[t + i] = lpz * env;
      }
    }
    all.insert(all.end(), out.begin(), out.end());
    printf("rendered %s\n", pt.name);
  }
  float pk = 0; for (float v : all) if (fabsf(v) > pk) pk = fabsf(v);
  FILE *f = fopen("demo_mono.raw", "wb"); const float g = 0.89f / pk;
  for (float v : all) { short a = (short)(v * g * 32767); fwrite(&a, 2, 1, f); } fclose(f);
  printf("peak %.2f, %.1f s\n", pk, all.size() / FS);
}
