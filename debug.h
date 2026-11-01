/* collision.h - fast 2D (XZ plane) oriented-box tests + response.
 *
 * Boxes: centre (x,z), heading yaw, half width hw (local X), half length hl (local Z).
 * Contact normals point from A toward B (box-vs-box) or INTO the road (walls).
 */
#ifndef COLLISION_H
#define COLLISION_H
#include <stdbool.h>
#include "car.h"
#include "traffic.h"
#include "track.h"

typedef struct { float x, z, yaw, hw, hl; } Box2;
typedef struct { float nx, nz, depth; } Contact;

bool box_vs_box(const Box2 *a, const Box2 *b, Contact *out);   /* SAT, 4 axes */
/* Track edges: the deepest box corner beyond the barrier's inner face. Normal points into the road. */
bool box_vs_track_walls(const Track *t, const Box2 *a, int hint, Contact *out);

/* Each returns the closing speed (m/s) of the hit, 0 if no contact. */
float collide_car_wall(Car *c, const Track *t, float dt);
float collide_car_traffic(Car *c, TrafficCar *t, float dt);

/* True once per pass when the player squeezes by a car at speed without touching. */
bool nearmiss_check(const Car *c, TrafficCar *t);

#endif
