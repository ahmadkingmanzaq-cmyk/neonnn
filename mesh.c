/* NEON RUSH - stage 3: spline track (checkpoints, racing line, tunnels, jumps, culling)
 * + driving model + traffic + collisions.
 *
 * Fixed-timestep sim order per tick:
 *   draft check -> car_step -> track projection + ground/jump -> traffic_update ->
 *   car/traffic collisions (+ near-miss) -> car/wall collision -> lap progress
 */
#include <pspkernel.h>
#include <pspdisplay.h>
#include <psppower.h>
#include <psprtc.h>
#include <math.h>
#include <string.h>
#include "config.h"
#include "mathutil.h"
#include "input.h"
#include "camera.h"
#include "renderer.h"
#include "world.h"
#include "debug.h"
#include "mesh.h"
#include "car.h"
#include "traffic.h"
#include "collision.h"
#include "track.h"

PSP_MODULE_INFO("NEON RUSH", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(20480);                       /* ~20 MB heap for the game */

#define START_S 8.0f                            /* spawn just past the start line */

/* ---- HOME-button exit handling ---- */
static int exit_callback(int a1, int a2, void *c) { (void)a1; (void)a2; (void)c; sceKernelExitGame(); return 0; }
static int callback_thread(SceSize args, void *argp) {
    (void)args; (void)argp;
    int id = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
    sceKernelRegisterExitCallback(id);
    sceKernelSleepThreadCB();
    return 0;
}
static void setup_callbacks(void) {
    int t = sceKernelCreateThread("cb_thread", callback_thread, 0x11, 0xFA0, 0, 0);
    if (t >= 0) sceKernelStartThread(t, 0, 0);
}

typedef struct { float impact, landing; int nearmisses, laps; } SimEvents;

static void sim_step(Car *car, Traffic *tr, const Track *trk, Progress *prog,
                     const Input *in, float dt, SimEvents *ev) {
    car->draft_target = (car->speed > 20.0f && traffic_draft_factor(tr, car) > 0.15f) ? 1.0f : 0.0f;
    car_step(car, in, dt);

    TrackPoint tp;                                       /* where are we on the road? */
    track_project(trk, car->pos.x, car->pos.z, car->seg, &tp);
    car->seg = tp.seg; car->s = tp.s; car->d = tp.d;
    float land = car_ground_update(car, tp.pos.y, dt);   /* follows hills, takes off at jumps */
    if (land > ev->landing) ev->landing = land;

    traffic_update(tr, dt, car, trk);
    for (int i = 0; i < MAX_TRAFFIC; i++) {
        TrafficCar *t = &tr->cars[i];
        float imp = collide_car_traffic(car, t, dt);
        if (imp > ev->impact) ev->impact = imp;
        if (nearmiss_check(car, t)) { ev->nearmisses++; car_add_nitro(car, 8.0f); }
    }
    float w = collide_car_wall(car, trk, dt);            /* walls last: traffic may have shoved us */
    if (w > ev->impact) ev->impact = w;
    car_on_hit(car, ev->impact);                         /* hard hits cancel drift + charge */

    if (progress_update(prog, trk, car->s, dt)) ev->laps++;
}

static void start_position(Car *car, const Track *trk, int type) {
    TrackPoint sp;
    track_at(trk, START_S, 0.0f, &sp);
    car_reset(car, type);
    car_place(car, sp.pos, sp.yaw);
    car->s = sp.s;  car->d = 0.0f;
}

int main(int argc, char **argv) {
    setup_callbacks();
    scePowerSetClockFrequency(333, 333, 166);   /* full CPU/bus speed */

    /* directory of the EBOOT, so data/ is found whether run from PPSSPP or a memory stick */
    char base[256] = "";
    if (argc > 0 && argv && argv[0]) {
        strncpy(base, argv[0], sizeof(base) - 1);
        char *slash = strrchr(base, '/');
        if (slash) slash[1] = 0; else base[0] = 0;
    }

    input_init();
    renderer_init();
    static Track track;
    track_load(&track, base, "data/tracks/neon_loop.trk");
    world_init(&track);

    int car_type = 0;
    Car car;  start_position(&car, &track, car_type);
    Camera cam;  camera_init(&cam, car.yaw);
    static Traffic traffic;  traffic_init(&traffic, &car, &track);
    Progress prog;  progress_reset(&prog, &track, car.s);
    Input in;
    float acc = 0, boost_vis = 0, drift_cam = 0;
    int show_debug = 1, paused = 0, fps = 0, frames = 0, nearmisses = 0;
    float fps_t = 0;

    u64 last; sceRtcGetCurrentTick(&last);
    const double res = (double)sceRtcGetTickResolution();

    for (;;) {
        u64 now; sceRtcGetCurrentTick(&now);
        float dt = clampf((float)((double)(now - last) / res), 0.0f, 0.1f);
        last = now;

        input_update(&in);
        if (in.pressed_debug) show_debug ^= 1;
        if (in.pressed_pause) paused ^= 1;
        if (in.pressed_hood)  cam.mode = (cam.mode == CAM_CHASE) ? CAM_HOOD : CAM_CHASE;
        if (in.pressed_remap) input_toggle_scheme();
        if (in.pressed_car)   { car_type = (car_type + 1) % NUM_CAR_TYPES; in.pressed_reset = true; }
        if (in.pressed_reset) {
            start_position(&car, &track, car_type);
            camera_init(&cam, car.yaw);
            traffic_init(&traffic, &car, &track);
            progress_reset(&prog, &track, car.s);
            nearmisses = 0;
        }

        SimEvents ev = { 0, 0, 0, 0 };
        if (!paused) {
            acc += dt;
            int steps = 0;
            while (acc >= SIM_DT && steps < 5) { sim_step(&car, &traffic, &track, &prog, &in, SIM_DT, &ev); acc -= SIM_DT; steps++; }
            if (steps == 5) acc = 0;                    /* spiral-of-death guard */
            boost_vis = damp(boost_vis, car_boost_visual(&car), 6.0f, dt);
        }
        nearmisses += ev.nearmisses;
        if (ev.impact > 2.0f)  camera_kick(&cam, clampf(ev.impact / 25.0f, 0.15f, 1.0f));
        if (ev.landing > 6.0f) camera_kick(&cam, clampf(ev.landing / 30.0f, 0.1f, 0.6f));

        float sf = clampf(fabsf(car.speed) / car_top_speed(&car), 0.0f, 1.0f);
        /* while drifting the camera leans toward the travel direction so the road stays in view */
        drift_cam = damp(drift_cam, car.drifting ? 0.45f : 0.0f, 6.0f, dt);
        float vel_yaw = (car.speed > 8.0f) ? atan2f(car.vx, -car.vz) : car.yaw;
        float cam_yaw = car.yaw + wrap_pi(vel_yaw - car.yaw) * drift_cam;
        camera_update(&cam, dt, car.pos, cam_yaw, sf, boost_vis, in.look_back);

        renderer_begin_frame(&cam);
        /* which side of the streaming window gets the long draw distance? compare the camera's
         * look direction with the track direction at the player */
        const TrackSample *ts = &track.s[car.seg >= 0 ? car.seg : 0];
        float lx = cam.target.x - cam.eye.x, lz = cam.target.z - cam.eye.z;
        int dir = (lx * ts->tx + lz * ts->tz >= 0.0f) ? 1 : -1;
        world_draw(&track, &car, dir, &traffic, show_debug != 0);
        if (show_debug) debug_draw(fps, (int)(fabsf(car.speed) * 3.6f), g_tri_count, nearmisses,
                                   car.type + 1, prog.laps_done + 1, (int)prog.lap_time);
        debug_bars(car.nitro / 100.0f, car.nitro_on, car.charge / 100.0f, car_tier(&car), car.draft);
        renderer_end_frame(TARGET_VBLANKS);

        frames++; fps_t += dt;
        if (fps_t >= 0.5f) { fps = (int)(frames / fps_t + 0.5f); frames = 0; fps_t = 0; }
    }
    return 0;
}
