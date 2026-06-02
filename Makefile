# --- TOOLCHAIN TOOLS ---
CC=avr-gcc
AS=avr-gcc

# --- DIRECTORY CONFIGURATION ---
SRC_DIR = src
KERNEL_DIR = $(SRC_DIR)/kernel
ARCH_DIR = $(SRC_DIR)/arch_avr

# --- DIRECTORY INCLUSIONS ---
# The -I flag tells the compiler where to look for header files (.h)
INCLUDE_DIRS = -I. -I$(SRC_DIR) -I$(KERNEL_DIR) -I$(ARCH_DIR)

# --- COMPILATION FLAGS ---
# CC_OPTS contains specific flags for bare-metal AVR development (ATmega2560)
# -MMD: Automatically generate dependency files (.d), ignoring system headers
# -MP: Adds dummy targets for each dependency to prevent errors if a header is deleted
CC_OPTS = -Wall --std=gnu99 -DF_CPU=16000000UL -O3 -funsigned-char \
          -funsigned-bitfields -fshort-enums -Wall -Wstrict-prototypes \
          -mmcu=atmega2560 $(INCLUDE_DIRS) -D__AVR_3_BYTE_PC__ \
          -MMD -MP

# AS_OPTS configures the assembler to handle C preprocessor macros
AS_OPTS = -x assembler-with-cpp $(CC_OPTS)

# --- AVRDUDE PROGRAMMER CONFIGURATION ---
AVRDUDE = avrdude
AVRDUDE_PORT = /dev/ttyACM0

AVRDUDE_FLAGS = -p m2560 -P $(AVRDUDE_PORT) -b 115200
AVRDUDE_FLAGS += $(AVRDUDE_NO_VERIFY) $(AVRDUDE_VERBOSE) $(AVRDUDE_ERASE_COUNTER)
AVRDUDE_FLAGS += -D -q -V -C /usr/share/arduino/hardware/tools/avr/../avrdude.conf
AVRDUDE_FLAGS += -c wiring

# --- OBJECT FILES LIST ---
OBJS = $(KERNEL_DIR)/tcb.o \
       $(KERNEL_DIR)/tcb_list.o \
       $(KERNEL_DIR)/scheduler.o \
       $(KERNEL_DIR)/semaphore.o \
       $(ARCH_DIR)/uart.o \
       $(ARCH_DIR)/atomport_asm.o \
       $(ARCH_DIR)/timer.o

# --- AUTOMATIC DEPENDENCY FILES LIST ---
# This matches every .o file in OBJS and converts its extension to .d
DEPS = $(OBJS:.o=.d) main.d

# --- OUTPUT BINARIES ---
TARGET_ELF = main.elf
TARGET_HEX = main.hex

.PHONY: clean all flash

# Default target triggered by running 'make'
all: $(TARGET_HEX)

# Pattern rule to compile C source files into object files
%.o: %.c
	$(CC) $(CC_OPTS) -c $< -o $@

# Pattern rule to assemble Assembly source files (.s) into object files
%.o: %.s
	$(AS) $(AS_OPTS) -c $< -o $@

# Linker rule: merges all object files together with the main application entry point
$(TARGET_ELF): $(SRC_DIR)/main.c $(OBJS)
	$(CC) $(CC_OPTS) -o $@ $< $(OBJS)

# Generates the final Intel HEX file required by the AVR microcontroller hardware
$(TARGET_HEX): $(TARGET_ELF)
	avr-objcopy -O ihex -R .eeprom $< $@

# Flash target: uploads the compiled HEX binary onto the Arduino Mega board via USB
flash: $(TARGET_HEX)
	$(AVRDUDE) $(AVRDUDE_FLAGS) -U flash:w:$(TARGET_HEX):i

# Clean target: removes all generated object files, binaries, and dependency files
clean:
	rm -f $(OBJS) $(TARGET_ELF) $(TARGET_HEX) $(DEPS) *~

# --- INCLUDE GENERATED DEPENDENCIES ---
# The '-' sign prevents make from throwing an error if the .d files do not exist yet (first build)
-include $(DEPS)