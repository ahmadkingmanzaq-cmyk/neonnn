# NEON RUSH - Stage 3 (spline track + driving model + traffic + collisions)

Build (PSPDEV toolchain on PATH, or inside the pspdev/pspdev docker image):
    make            # -> EBOOT.PBP
    make clean

Run:
  PPSSPP : drag EBOOT.PBP onto the window.
  PSP    : copy EBOOT.PBP AND the data/ folder to ms0:/PSP/GAME/NEON_RUSH/
           (if data/ is missing the game falls back to a built-in copy of the track)
           (ef0:/PSP/GAME/ on a PSP Go), run from Game > Memory Stick (ARK-4).

Stage-1 test controls:
  R accel | L brake/reverse | Cross nitro | Circle drift | analog / D-pad L-R steer
  Triangle (hold) look back | D-pad Up hood cam | D-pad Down swap car (AURORA GT / KESTREL R)
  Triangle + D-pad Down swap button layout | Square reset | Start pause | Select debug overlay
  Overlay: GREEN = FPS, CYAN = km/h, YELLOW = triangles drawn

Traffic & collisions:
  14 civilian cars share the road (same direction, 4 lanes), follow each other, change
  lanes, and brake for you. Hit them or the barriers: shove/spin, speed loss, camera shake.
  Overlay MAGENTA number = near misses (pass a car within ~1.8 m at speed without touching).

Driving model (stage 2):
  DRIFT   hold Circle above 40% top speed while steering. Grip drops, direction locks to
          the side you entered; stick tightens/widens the arc. Brake or hit a wall = cancel.
  CHARGE  fills while slip angle > ~12 deg. Bar ticks at 25/55/90 = tier 1/2/3.
  RELEASE let go of Circle: burst of 0.8 / 1.4 / 2.0 s (+20% top speed, 2x accel).
  NITRO   cyan bar. Fills from drifting, near misses (+8) and slipstream (sit 3-20 m behind
          a car, centred; white blip). Cross spends it: +35% top speed, empty in ~3 s.
  Bonuses stack but total top-speed multiplier is capped at 1.5.
  Bars: bottom-left (top = drift charge, bottom = nitro).

Track system (stage 3):
  data/tracks/neon_loop.trk is a list of pieces, integrated into a closed loop of spline
  samples (~4 m apart) at load time:
      straight <len> [tunnel] [jump <h>] [hill <h>]
      curve <angle> <radius> [tunnel]        (+ = right, - = left)
      halfwidth / spacing / checkpoints / name
  Total turning should be 360 deg; leftover position error is spread along the lap.
  The loader derives curvature, slope, a racing line (lateral offset + target speed with a
  braking pass) and evenly spaced checkpoint gates (gate 0 = start/finish).
  Rendering streams only the ~11 chunks (24 m each) around the player; fog hides the cutoff.
  Select overlay also shows the racing line (green = fast, red = slow) and lap numbers:
  ORANGE = lap, dim-green = current lap time in seconds.
  Jumps: the road drops away after a ramp; the car goes airborne (no steering/grip) and
  lands with a camera kick. Hills are smooth and never launch the car.
  If you edit the .trk file, also update src/track_default.inc (the built-in fallback copy).
