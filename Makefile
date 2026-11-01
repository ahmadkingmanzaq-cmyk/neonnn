TARGET = neon_rush
OBJS = src/main.o src/mathutil.o src/input.o src/renderer.o src/mesh.o \
       src/camera.o src/world.o src/debug.o src/car.o src/collision.o \
       src/traffic.o src/track.o src/track_render.o

INCDIR =
CFLAGS = -O2 -G0 -Wall -ffast-math -fno-strict-aliasing
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBDIR =
LIBS = -lpspgum -lpspgu -lpspge -lpspdisplay -lpspctrl -lpsprtc -lpsppower -lm

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = NEON RUSH
# PSP_EBOOT_ICON = ICON0.PNG
# PSP_EBOOT_PIC1 = PIC1.PNG

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
