
# Quiet (set to @ for a quite compile)
Q	?= @
#Q	?=

# Build Tools
CC 	:= gcc
CFLAGS += -I. -Wall -funroll-loops -ffast-math -fPIC -DPIC -O0 -g
LD := gcc
LDFLAGS += -Wall -shared -lasound

SND_CONF_OBJECTS = conf_ec.o
SND_CONF_LIBS =
SND_CONF_BIN = libasound_module_conf_ec.so


MULTIARCH:=$(shell gcc --print-multiarch)
LIBDIR = lib/$(MULTIARCH)

.PHONY: all clean dep load_default

all: Makefile $(SND_CONF_BIN) $(SND_CTL_BIN)

dep:
	@echo DEP $@
	$(Q)for i in *.c; do $(CC) -MM $(CFLAGS) "$${i}" ; done > makefile.dep

-include makefile.dep

$(SND_CONF_BIN): $(SND_CONF_OBJECTS)
	@echo LD $@
	$(Q)$(LD) $(LDFLAGS) $(SND_CONF_LIBS) $(SND_CONF_OBJECTS) -o $(SND_CONF_BIN)

%.o: %.c
	@echo GCC $<
	$(Q)$(CC) -c $(CFLAGS) $(CPPFLAGS) $<

clean:
	@echo Cleaning...
	$(Q)rm -vf *.o *.so

install: all
	@echo Installing...
	$(Q)mkdir -p ${DESTDIR}/usr/$(LIBDIR)/alsa-lib/
	$(Q)install -m 644 $(SND_CONF_BIN) ${DESTDIR}/usr/$(LIBDIR)/alsa-lib/

uninstall:
	@echo Un-installing...
	$(Q)rm ${DESTDIR}/usr/$(LIBDIR)/alsa-lib/$(SND_CONF_BIN)
