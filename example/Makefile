ifneq ($(CROSS_COMPILE),)
CROSS-COMPILE:=$(CROSS_COMPILE)
endif
#CROSS-COMPILE:=/home/kpkang/downloads/buildroot/output/host/usr/bin/arm-buildroot-linux-uclibcgnueabihf-
#CROSS-COMPILE:=/workspace/buildroot/buildroot-qemu_mips_malta_defconfig/output/host/usr/bin/mips-buildroot-linux-uclibc-
#CROSS-COMPILE:=/workspace/buildroot/buildroot-qemu_arm_vexpress_defconfig/output/host/usr/bin/arm-buildroot-linux-uclibcgnueabi-
#CROSS-COMPILE:=/workspace/buildroot-git/qemu_mips64_malta/output/host/usr/bin/mips-gnu-linux-
ifeq ($(CC),cc)
CC:=$(CROSS-COMPILE)gcc
endif
LD:=$(CROSS-COMPILE)ld
AR:= $(CROSS-COMPILE)ar
STRIP:= $(CROSS-COMPILE)strip

CFLAGS  += -I../libs

OBJS		= example.o

DEFS		=
APP		 := example

all:	$(APP) $(INCLUDES)

${APP}:	$(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) -lril -L../libs -lpthread
#	cp $@ /shared/

clean:
	-rm -f *.o $(APP)
