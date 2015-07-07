ARCH = arm-none-eabi
CC = $(ARCH)-gcc
AR = $(ARCH)-ar
RANLIB = $(ARCH)-ranlib

CFLAGS =

TARGETS = libSceLibKernel.a libSceIoFilemgr.a

SceLibKernel_OBJS = SceLibKernel.o SceLibGcc.o
SceIoFilemgr_OBJS =
ALL_OBJS=

all: $(TARGETS)

define LIBRARY_template
 $(1): $$($(1:lib%.a=%)_OBJS)
 ALL_OBJS += $$($(1:lib%.a=%)_OBJS)
endef

$(foreach library,$(TARGETS),$(eval $(call LIBRARY_template,$(library))))

all: $(TARGETS)

clean:

	rm -f $(TARGETS) $(ALL_OBJS)

$(TARGETS):
	$(AR) cru $@ $?
	$(RANLIB) $@

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@
