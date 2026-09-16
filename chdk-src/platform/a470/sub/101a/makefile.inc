# a470 - 101a uses 101b source (identical firmware)

TARGET_FW:=$(PLATFORMSUB)

override PLATFORMSUB=101b

include $(camfw)/makefile.inc
