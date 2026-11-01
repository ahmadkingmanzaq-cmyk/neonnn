/* input.h - pad polling, deadzone/curve, button schemes. */
#ifndef INPUT_H
#define INPUT_H
#include <stdbool.h>

typedef struct {
    float steer;        /* -1 (left) .. +1 (right) */
    float throttle;     /* 0 or 1 */
    float brake;        /* 0 or 1 */
    bool  boost, drift, look_back;
    /* one-frame edge events */
    bool  pressed_pause, pressed_debug, pressed_hood, pressed_remap, pressed_car, pressed_reset;
} Input;

void input_init(void);
void input_update(Input *in);
int  input_scheme(void);          /* 0 = R/L triggers, 1 = Cross/Square layout */
void input_toggle_scheme(void);

#endif
