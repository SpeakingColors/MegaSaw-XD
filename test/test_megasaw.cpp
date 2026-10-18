// PC sanity test for MegaSaw XD: every wave, every voice count, three octaves.
// Checks: no NaN/inf, no clipping past full scale, DC offset small, loudness consistent.
#include "../megasaw.cpp"
#include <stdio.h>
#include <vector>
static std::vector<float> render(int midi, float sec) {
  user_osc_param_t p = {0, (uint16_t)(midi << 8), 0, 0};
  OSC_NOTEON(&p);
  const int N = (int)(sec * 48000), F = 32;
  std::vector<float> out(N); int32_t buf[F];
  for (int t = 0; t < N; t += F) { OSC_CYCLE(&p, buf, F); for (int i = 0; i < F && t + i < N; i++) out[t + i] = buf[i] / 2147483648.f; }
  return out;
}
static void stats(const std::vector<float> &x, float &rms, float &peak, float &dc, bool &bad) {
  double s2 = 0, s1 = 0; peak = 0; bad = false;
  for (float v : x) { if (!(v == v) || fabsf(v) > 1.f) bad = true; s2 += v * v; s1 += v; if (fabsf(v) > peak) peak = fabsf(v); }
  rms = (float)sqrt(s2 / x.size()); dc = (float)(s1 / x.size());
}
int main() {
  int fails = 0; const char *wn[] = {"Off","Saw","Square","Pulse25","Pulse12","Tri","Sine","Glass1","Glass2","VoxAh"};
  printf("--- layer A alone, 7 voices, one wave at a time (RMS in dBFS, note C2/C3/C4) ---\n");
  for (int w = 1; w <= 9; w++) {
    printf("%-8s", wn[w]);
    for (int midi : {36, 48, 60}) {
      OSC_INIT(0, 0); OSC_PARAM(0, w); OSC_PARAM(1, 0); OSC_PARAM(2, 100); OSC_PARAM(3, 100); OSC_PARAM(4, 100); OSC_PARAM(5, 0);
      OSC_PARAM(6, 600); OSC_PARAM(7, 500);
      auto x = render(midi, 1.0f); float r, pk, dc; bool bad; stats(x, r, pk, dc, bad);
      printf("  %6.1f dB pk %.2f%s", 20 * log10f(r + 1e-9f), pk, bad ? " BAD" : "");
      if (bad || fabsf(dc) > 0.02f) fails++;
    }
    printf("\n");
  }
  printf("--- all voice counts, Saw+Saw (B at +1 octave, Bal 0), drift 50%%, detune 100%% ---\n");
  float vmin = 1e9f, vmax = -1e9f;
  for (int vi = -2; vi <= 2; vi++) {
    OSC_INIT(0, 0); OSC_PARAM(0, 1); OSC_PARAM(1, 1); OSC_PARAM(2, 101); OSC_PARAM(3, 100); OSC_PARAM(4, 100 + vi); OSC_PARAM(5, 50);
    OSC_PARAM(6, 1023); OSC_PARAM(7, 500);
    auto x = render(60, 1.0f); float r, pk, dc; bool bad; stats(x, r, pk, dc, bad);
    const float db = 20 * log10f(r + 1e-9f);
    printf("%2d voices: %6.1f dB peak %.2f dc %.4f%s\n", 7 + 2 * vi, db, pk, dc, bad ? " BAD" : "");
    if (bad) fails++;
    if (db < vmin) vmin = db;
    if (db > vmax) vmax = db;
  }
  printf("voice-count loudness spread: %.1f dB (must stay under 2.0)\n", vmax - vmin);
  if (vmax - vmin > 2.0f) fails++;
  printf("--- extremes: bal -100/+100, Oct B -2/+2, all-Off fallback, Vox+Vox at 11 voices ---\n");
  struct C { int a, b, ob, bal, vi; } cs[] = {{1,1,98,0,2},{1,1,102,0,2},{1,1,100,0,0},{1,1,100,200,0},{0,0,100,100,0},{9,9,100,100,2},{9,1,99,80,2}};
  for (auto c : cs) {
    OSC_INIT(0, 0); OSC_PARAM(0, c.a); OSC_PARAM(1, c.b); OSC_PARAM(2, c.ob); OSC_PARAM(3, c.bal); OSC_PARAM(4, 100 + c.vi); OSC_PARAM(5, 100);
    OSC_PARAM(6, 1023); OSC_PARAM(7, 1023);
    for (int midi : {24, 60, 96, 120}) {
      auto x = render(midi, 0.5f); float r, pk, dc; bool bad; stats(x, r, pk, dc, bad);
      if (bad || r < 1e-4f) { printf("FAIL a=%d b=%d ob=%d bal=%d vi=%d note %d: rms %.5f%s\n", c.a, c.b, c.ob, c.bal, c.vi, midi, r, bad ? " BAD" : " silent"); fails++; }
    }
  }
  printf("%s (%d failures)\n", fails ? "FAILED" : "ALL OK", fails);
  return fails != 0;
}
