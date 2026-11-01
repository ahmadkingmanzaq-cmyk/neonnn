#include <pspctrl.h>
#include "input.h"
#include "mathutil.h"

static int      s_scheme = 0;
static unsigned s_prev   = 0;

void input_init(void) {
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
}

int  input_scheme(void)        { return s_scheme; }
void input_toggle_scheme(void) { s_scheme ^= 1; }

void input_update(Input *in) {
    SceCtrlData pad;
    sceCtrlPeekBufferPositive(&pad, 1);
    unsigned b  = pad.Buttons;
    unsigned pr = b & ~s_prev;          /* newly pressed this frame */
    s_prev = b;

    /* Analog nub: centre 128, nub is noisy so use a deadzone, then a
     * squared response curve for fine control near the centre. */
    const float DZ = 0.16f;
    float x  = ((float)pad.Lx - 128.0f) / 127.0f;
    float ax = fabsf(x);
    x = (ax < DZ) ? 0.0f : ((x > 0 ? 1.0f : -1.0f) * ((ax - DZ) / (1.0f - DZ)));
    x = x * fabsf(x);
    if (b & PSP_CTRL_LEFT)  x = -1.0f;   /* D-pad = alternate steering */
    if (b & PSP_CTRL_RIGHT) x =  1.0f;
    in->steer = clampf(x, -1.0f, 1.0f);

    unsigned accel, brake, boost, drift, reset;
    if (s_scheme == 0) {            /* spec layout */
        accel = PSP_CTRL_RTRIGGER; brake = PSP_CTRL_LTRIGGER;
        boost = PSP_CTRL_CROSS;    drift = PSP_CTRL_CIRCLE;  reset = PSP_CTRL_SQUARE;
    } else {                        /* face-button layout */
        accel = PSP_CTRL_CROSS;    brake = PSP_CTRL_SQUARE;
        boost = PSP_CTRL_CIRCLE;   drift = PSP_CTRL_RTRIGGER; reset = PSP_CTRL_LTRIGGER;
    }
    in->throttle  = (b & accel) ? 1.0f : 0.0f;
    in->brake     = (b & brake) ? 1.0f : 0.0f;
    in->boost     = (b & boost) != 0;
    in->drift     = (b & drift) != 0;
    in->look_back = (b & PSP_CTRL_TRIANGLE) != 0;

    in->pressed_reset  = (pr & reset) != 0;
    in->pressed_pause  = (pr & PSP_CTRL_START)  != 0;
    in->pressed_debug  = (pr & PSP_CTRL_SELECT) != 0;
    in->pressed_hood   = (pr & PSP_CTRL_UP)     != 0;   /* stage-1 test binding */
    /* D-pad Down = swap car (test binding); Triangle + Down = swap button layout */
    in->pressed_remap  = (pr & PSP_CTRL_DOWN) && (b & PSP_CTRL_TRIANGLE);
    in->pressed_car    = (pr & PSP_CTRL_DOWN) && !(b & PSP_CTRL_TRIANGLE);
}
