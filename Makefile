# Apply different gcc optimization flags (-Ox and -fxxx)
# Initially, the flags are configured for no optimization (to enable better
# debugging) but you can experiment with different compiler settings
# (e.g. different levels and enabling/disabling specific optimizations)
bump.o: CFLAGS += -O2
implicit.o: CFLAGS += -O2
explicit.o: CFLAGS += -O2

ALLOCATORS = bump implicit explicit
PROGRAMS = $(ALLOCATORS:%=test_%)
MY_PROGRAMS = $(ALLOCATORS:%=my_optional_program_%)

all:: $(PROGRAMS) $(MY_PROGRAMS)

CC = gcc
CFLAGS = -g3 -std=gnu99 -Wall $$warnflags
export warnflags = -Wfloat-equal -Wtype-limits -Wpointer-arith -Wlogical-op -Wshadow -Winit-self -fno-diagnostics-show-option
LDFLAGS =
LDLIBS =

$(PROGRAMS): test_%:%.o segment.c test_harness.c
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(MY_PROGRAMS): my_optional_program_%:my_optional_program.c %.o segment.c
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean::
	rm -f $(PROGRAMS) $(MY_PROGRAMS) *.o callgrind.out.*

.PHONY: clean all

.INTERMEDIATE: $(ALLOCATORS:%=%.o)
