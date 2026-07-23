$(info -- MMCEMAN)
#Build full MMCEMAN for use with loader / homebrew
PS2SDKSRC ?= $(PS2SDK)
IOP_BIN ?= mmceman.irx
IOP_SRC_DIR ?= src/
IOP_INC_DIR ?= include/

MMCEMAN_OBJS = ioplib.o sio2man_hook.o mmce_sio2.o mmce_cmds.o mmce_fs.o imports.o mmceman.o
MMCEMAN_IMPORT_INCS = \
	sio/sio2man \
	system/dmacman \
	system/intrman \
	system/iomanx \
	system/loadcore \
	system/stdio \
	system/sysclib \
	system/threadman

ifneq ($(strip $(IOP_OBJS_DIR)),)
IOP_OBJS_DIR := $(patsubst %/,%,$(IOP_OBJS_DIR))/
IOP_OBJS = $(addprefix $(IOP_OBJS_DIR),$(MMCEMAN_OBJS))
$(IOP_OBJS): | $(IOP_OBJS_DIR)
else
IOP_OBJS = $(MMCEMAN_OBJS)
endif

#Needed for iomanX
IOP_CFLAGS += -DIOMANX_OLD_NAME_COMPATIBILITY=0 -DIOMANX_OLD_NAME_ADDDELDRV=0
IOP_INCS += -I$(IOP_INC_DIR)
ifneq ($(strip $(PS2SDKSRC)),)
IOP_INCS += -I$(PS2SDKSRC)/iop/kernel/include -I$(PS2SDKSRC)/common/include
IOP_INCS += $(addprefix -I$(PS2SDKSRC)/iop/, $(addsuffix /include,$(MMCEMAN_IMPORT_INCS)))
endif

all: $(IOP_BIN)

clean:
	rm -f -r $(IOP_OBJS) $(IOP_BIN) $(IOP_BIN:.irx=.notiopmod.elf) $(IOP_BIN:.irx=.notiopmod.stripped.elf)

ifeq ($(DEBUG), 1)
  $(info -- MMCEMAN debug enabled)
  IOP_CFLAGS += -DDEBUG
  IOP_CFLAGS += -DDEBUG=1
endif

include $(PS2SDK)/Defs.make
include ../Rules.make
