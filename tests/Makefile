# PC test suite for the I2C flash bootloader.
# Compiles src/hex_parser.c and src/flash.c for the host against mocked AVR
# registers and a simulated SPM flash (tests/mock).
# No microcontroller or programmer required.
#
#   make -C tests          # build + run C unit tests and test_hex_parser.py
#   make -C tests avr      # additionally check the FBL builds with avr-gcc and fits the boot section
#   make -C tests clean

CC      ?= gcc
CFLAGS  ?= -O0 -g -Wall -Wextra -Wno-unused-parameter
CFLAGS  += -std=gnu99 -funsigned-char -D__AVR_ATmega8__ -DF_CPU=8000000UL -DSPM_PAGESIZE=64
CFLAGS  += -fsanitize=address,undefined -fno-omit-frame-pointer
INC     := -Imock -I../src
BUILD   := build
BIN     := $(BUILD)/test_fbl
HDRS    := $(wildcard ../src/*.h mock/*.h mock/avr/*.h) minitest.h
SRCS    := test_fbl.c mock/fbl_mock.c ../src/flash.c ../src/hex_parser.c

# Boot section: 1024 words (BOOTSZ for 0x1800 start on atmega8)
BOOT_SECTION_BYTES := 2048

.PHONY: all test avr clean
all: test

$(BIN): $(SRCS) $(HDRS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(INC) -o $@ $(SRCS)

test: $(BIN)
	./$(BIN)
	python3 test_hex_parser.py

avr:
	$(MAKE) -C ../Release clean
	$(MAKE) -C ../Release all
	@size=$$(avr-size -A ../Release/atmega_i2c_fbl.elf | awk '$$1==".text"||$$1==".data"{s+=$$2} END{print s}'); \
	echo "FBL size: $$size bytes (boot section: $(BOOT_SECTION_BYTES) bytes)"; \
	test $$size -le $(BOOT_SECTION_BYTES) || { echo "ERROR: FBL does not fit into boot section"; exit 1; }

clean:
	rm -rf $(BUILD)
