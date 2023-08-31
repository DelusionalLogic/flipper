CC ?= gcc

OBJDIR ?= obj

LIBS = -lm
INCS =

CFLAGS ?= -O3 -D_FORTIFY_SOURCE=2 -Wall -g
CFLAGS += -std=gnu11

print-%  : ; @echo $* = $($*)

SOURCES = main.c

PACKAGES=libevdev
ifeq "$(RENDER)" "SDL"
	CFLAGS += -DRENDER=SDL
	PACKAGES += sdl
	SOURCES += sdl.c
else ifeq "$(RENDER)" "FB"
	CFLAGS += -DRENDER=FB
	SOURCES += fb.c
endif

LIBS += $(shell pkg-config --libs $(PACKAGES))
INCS += $(shell pkg-config --cflags $(PACKAGES))
OBJS = $(SOURCES:%.c=$(OBJDIR)/%.o)

-include $(shell find $(OBJDIR) -name "*.d")

# We don't really need to run the tests for bear to record them
compile_commands.json: clean Makefile
	@rm -f "$@"
	bear -- make

main: $(OBJS)
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $^ $(LIBS)

$(OBJDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCS) -MMD -o $@ -c $<

clean:
	@rm -rf $(OBJDIR)
	@rm -f main

.DEFAULT_GOAL := all
all: main
