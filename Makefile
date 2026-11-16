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

SRCS = $(addprefix src/,config_file.c field.c fontfind.c length.c palette.c setting.c speech.c)
OBJS = $(SRCS:src/%.c=build/%.o)

# Text layout (textlayout.c) needs FreeType, and HarfBuzz and FriBidi unless it is built with -DNO_I18N (the
# plain flavour, which a tool built with I18N=0 gets). It is built and tested where those are installed.
TEXT_PKGS = freetype2 harfbuzz fribidi
HAVE_TEXT := $(shell pkg-config --exists $(TEXT_PKGS) 2>/dev/null && echo 1)
ifeq ($(HAVE_TEXT),1)
TEXT_CFLAGS := $(shell pkg-config --cflags $(TEXT_PKGS))
TEXT_LIBS := $(shell pkg-config --libs $(TEXT_PKGS))
PLAIN_CFLAGS := $(shell pkg-config --cflags freetype2)
PLAIN_LIBS := $(shell pkg-config --libs freetype2)
OBJS += build/textlayout.o build/textlayout-plain.o
TEXT_TESTS = text-test
endif

all: $(OBJS)

build/%.o: src/%.c | build
	$(CC) $(ALL_CFLAGS) -c $< -o $@

build/textlayout.o: src/textlayout.c | build
	$(CC) $(ALL_CFLAGS) $(TEXT_CFLAGS) -c $< -o $@

build/textlayout-plain.o: src/textlayout.c | build
	$(CC) $(ALL_CFLAGS) $(PLAIN_CFLAGS) -DNO_I18N -c $< -o $@

build:
	mkdir -p build

build/test: tests/test.c $(filter-out build/textlayout.o build/textlayout-plain.o,$(OBJS))
	$(CC) $(ALL_CFLAGS) $(ALL_LDFLAGS) $^ -o $@ $(LDLIBS) -lm

build/text-test: tests/text_test.c build/textlayout.o build/fontfind.o | build
	$(CC) $(ALL_CFLAGS) $(TEXT_CFLAGS) $(ALL_LDFLAGS) $^ -o $@ $(TEXT_LIBS) -lm

build/text-test-plain: tests/text_test.c build/textlayout-plain.o build/fontfind.o | build
	$(CC) $(ALL_CFLAGS) $(PLAIN_CFLAGS) -DNO_I18N $(ALL_LDFLAGS) $^ -o $@ $(PLAIN_LIBS) -lm

test: build/test $(if $(HAVE_TEXT),build/text-test build/text-test-plain)
	./build/test
ifeq ($(HAVE_TEXT),1)
	./build/text-test
	./build/text-test-plain
else
	@echo "text layout not tested: HarfBuzz, FriBidi or FreeType not found by pkg-config"
endif

# The tests again under AddressSanitizer and UBSan.
sanitize:
	mkdir -p build
	$(CC) -g -fsanitize=address,undefined -Isrc $(SRCS) tests/test.c -o build/test-sanitize -lm
	./build/test-sanitize
ifeq ($(HAVE_TEXT),1)
	$(CC) -g -fsanitize=address,undefined -Isrc $(TEXT_CFLAGS) src/textlayout.c src/fontfind.c tests/text_test.c -o build/text-test-sanitize $(TEXT_LIBS) -lm
	./build/text-test-sanitize
	$(CC) -g -fsanitize=address,undefined -Isrc $(PLAIN_CFLAGS) -DNO_I18N src/textlayout.c src/fontfind.c tests/text_test.c -o build/text-test-plain-sanitize $(PLAIN_LIBS) -lm
	./build/text-test-plain-sanitize
endif

clean:
	rm -rf build

-include $(OBJS:.o=.d)

.PHONY: all test sanitize clean
