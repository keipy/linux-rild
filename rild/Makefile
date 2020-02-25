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

CSOURCE += agent.c \
	cmdlist.c \
	finalresult.c \
	inforesponse.c \
	unsolicited.c \
	oemlayer.c \
	clientIF.c \
	sms.c \
	serialport.c \
	utils.c \
	ksc2ucs.c

#CFLAGS += -I$(ROOTDIR)/$(LINUXDIR)/drivers/char -I../include
CFLAGS += -I../libs -DSUPPORT_TCP_CMD -DSUPPORT_VOICE_CALL
#CFLAGS += -I../libs -DSUPPORT_TCP_CMD -DSUPPORT_MODEM_SLEEP -DSUPPORT_VOICE_CALL

release: clean
	$(CC) $(CFLAGS) -Wall -s $(CSOURCE) -o rild -lpthread

debug: clean
	$(CC) $(CFLAGS) -Wall -g $(CSOURCE) -o rild -lpthread

clean:
	rm -rf rild

