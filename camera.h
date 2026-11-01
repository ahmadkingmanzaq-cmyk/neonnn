# NEON LOOP - NEON RUSH track definition (units: metres, degrees)
#
# name <text>
# halfwidth <m>          road half width (default 8)
# spacing <m>            target distance between spline samples (default 4)
# checkpoints <n>        evenly spaced gates; gate 0 is the start/finish line
#
# straight <len> [tunnel] [jump <h>] [hill <h>]
#     jump: ramp of h metres over 30 m, then the road drops away over 16 m (launches the car)
#     hill: smooth bump of h metres spanning most of the piece
# curve <angle> <radius> [tunnel]     angle > 0 turns right, < 0 turns left
#
# The loader closes the loop automatically (total turning should be 360 deg; a small
# position error is spread along the lap), builds the racing line from curvature
# and generates all geometry.

name NEON LOOP
halfwidth 8
spacing 4
checkpoints 8

straight 260 jump 3
curve 90 110
straight 300 tunnel
curve 90 110
straight 160
curve -45 100
curve 45 100
straight 198.8 hill 4
curve 90 110
straight 100
curve 30 150
curve -30 150
straight 108.6
curve 90 110
straight 200
