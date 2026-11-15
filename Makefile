# pacommon is not installed: each pac tool compiles these files from ../pacommon. This Makefile
# builds them with the same hardening flags the tools use, and runs their tests.
CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra

# Warnings that guard against overflows and bad data: no variable-length arrays (a size that comes
# from input), format strings that are not literals, and shadowed names. Added to CFLAGS, so a user
# who sets their own CFLAGS still gets them.
GUARD_CFLAGS = -Wvla -Wformat=2 -Wshadow -Wstrict-prototypes -Wundef

# Hardening, as in the tools. Control-flow protection only exists on some compilers: ask the
# compiler rather than guess from the architecture.
CET_CFLAGS := $(shell echo 'int main(void){return 0;}' | $(CC) -x c - -fcf-protection=full \
                -o /dev/null 2>/dev/null && echo -fcf-protection=full)
ALL_CFLAGS := $(CFLAGS) $(GUARD_CFLAGS) -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3 -fstack-protector-strong \
              -fstack-clash-protection $(CET_CFLAGS) -fPIE -MMD -MP -Isrc
ALL_LDFLAGS := $(LDFLAGS) -Wl,--as-needed -pie -Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack

SRCS = $(addprefix src/,config_file.c field.c fontfind.c palette.c setting.c speech.c)
OBJS = $(SRCS:src/%.c=build/%.o)

all: $(OBJS)

build/%.o: src/%.c | build
	$(CC) $(ALL_CFLAGS) -c $< -o $@

build:
	mkdir -p build

build/test: tests/test.c $(OBJS)
	$(CC) $(ALL_CFLAGS) $(ALL_LDFLAGS) $^ -o $@ $(LDLIBS) -lm

test: build/test
	./build/test

# The tests again under AddressSanitizer and UBSan.
sanitize:
	mkdir -p build
	$(CC) -g -fsanitize=address,undefined -Isrc $(SRCS) tests/test.c -o build/test-sanitize -lm
	./build/test-sanitize

clean:
	rm -rf build

-include $(OBJS:.o=.d)

.PHONY: all test sanitize clean
