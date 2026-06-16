// Sample voice DSP test (PR B: sample engine).
//
// Pure-function coverage of src/sample_voice.h -- the pitch/advance/read
// math behind SampleOutputDevice. SDL-free, no audio device: compiles only
// the header + src/sample.cpp.

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "sample.h"
#include "sample_voice.h"

static int failures = 0;
static int checks   = 0;

#define CHECK(expr) do {                                                \
    checks++;                                                           \
    if (!(expr)) { failures++;                                          \
        fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr); \
    }                                                                   \
} while(0)

#define CLOSE(a,b) (fabs((double)(a) - (double)(b)) < 1e-6)

static void mk(zt_sample *s, unsigned int frames, unsigned char ch, unsigned int rate,
               unsigned char root) {
    s->clear();
    s->rate = rate; s->channels = ch; s->root_note = root; s->finetune = 0;
    short int pcm[512];
    unsigned int total = frames * ch;
    for (unsigned int i = 0; i < total && i < 512; i++) pcm[i] = (short int)(i + 1);
    s->set_pcm(pcm, frames, ch);
}

static void test_step_pitch() {
    zt_sample s; mk(&s, 64, 1, 44100, 60);

    // root note at output rate == 1.0 frame/frame
    CHECK(CLOSE(zt_voice_step(&s, 60, 44100), 1.0));
    // one octave up == 2x
    CHECK(CLOSE(zt_voice_step(&s, 72, 44100), 2.0));
    // one octave down == 0.5x
    CHECK(CLOSE(zt_voice_step(&s, 48, 44100), 0.5));

    // sample rate below output rate scales the base ratio
    zt_sample h; mk(&h, 64, 1, 22050, 60);
    CHECK(CLOSE(zt_voice_step(&h, 60, 44100), 0.5));
    CHECK(CLOSE(zt_voice_step(&h, 72, 44100), 1.0));   // octave up cancels the half-rate

    // invalid inputs
    CHECK(zt_voice_step(NULL, 60, 44100) == 0.0);
    CHECK(zt_voice_step(&s, 60, 0) == 0.0);
}

static void test_finetune() {
    zt_sample s; mk(&s, 64, 1, 44100, 60);
    s.finetune = 100;   // +100 cents == +1 semitone
    CHECK(CLOSE(zt_voice_step(&s, 60, 44100), pow(2.0, 1.0/12.0)));
}

static void test_advance_no_loop_ends() {
    zt_sample s; mk(&s, 10, 1, 44100, 60);   // no loop flag
    bool active = true;
    double pos = 9.0;
    pos = zt_voice_advance(&s, pos, 0.5, &active);   // 9.5, still inside
    CHECK(active);
    pos = zt_voice_advance(&s, pos, 1.0, &active);   // 10.5 -> past end
    CHECK(!active);
}

static void test_advance_forward_loop_wraps() {
    zt_sample s; mk(&s, 100, 1, 44100, 60);
    s.flags = ZTM_SMPF_LOOP; s.loop_start = 20; s.loop_end = 30;  // 10-frame loop
    bool active = true;
    double pos = 29.0;
    pos = zt_voice_advance(&s, pos, 3.0, &active);   // 32 -> wrap: 20 + (32-30)%10 = 22
    CHECK(active);
    CHECK(CLOSE(pos, 22.0));
    // a huge step still lands inside the loop region
    pos = zt_voice_advance(&s, 25.0, 137.0, &active);
    CHECK(active);
    CHECK(pos >= 20.0 && pos < 30.0);
}

static void test_read_interpolation() {
    zt_sample s; mk(&s, 8, 1, 44100, 60);   // pcm[i] = i+1 -> 1,2,3,...,8
    CHECK(CLOSE(zt_voice_read(&s, 0.0, 0), 1.0));
    CHECK(CLOSE(zt_voice_read(&s, 1.0, 0), 2.0));
    CHECK(CLOSE(zt_voice_read(&s, 0.5, 0), 1.5));     // halfway between 1 and 2
    CHECK(CLOSE(zt_voice_read(&s, 2.25, 0), 3.25));   // 3 + .25*(4-3)
    // mono returns the same value for both channels
    CHECK(CLOSE(zt_voice_read(&s, 3.0, 0), zt_voice_read(&s, 3.0, 1)));
    // past the end clamps to the last frame
    CHECK(CLOSE(zt_voice_read(&s, 100.0, 0), 8.0));
    // empty sample reads as silence
    zt_sample e; e.clear();
    CHECK(zt_voice_read(&e, 0.0, 0) == 0.0f);
}

static void test_read_stereo_channels() {
    zt_sample s; mk(&s, 4, 2, 44100, 60);   // interleaved: (1,2)(3,4)(5,6)(7,8)
    CHECK(CLOSE(zt_voice_read(&s, 0.0, 0), 1.0));   // L
    CHECK(CLOSE(zt_voice_read(&s, 0.0, 1), 2.0));   // R
    CHECK(CLOSE(zt_voice_read(&s, 1.0, 0), 3.0));
    CHECK(CLOSE(zt_voice_read(&s, 1.0, 1), 4.0));
}

int main(void) {
    test_step_pitch();
    test_finetune();
    test_advance_no_loop_ends();
    test_advance_forward_loop_wraps();
    test_read_interpolation();
    test_read_stereo_channels();
    printf("sample_voice: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
