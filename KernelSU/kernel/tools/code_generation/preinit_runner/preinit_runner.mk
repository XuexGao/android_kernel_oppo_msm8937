# preinit runner: a static nolibc binary embedded into kernelsu, see preinit/.
# It is built against the installed UAPI headers of the target kernel
# (CONFIG_HEADERS_INSTALL, enabled by gki_defconfig).
KSU_UAPI_INC := $(objtree)/usr/include

# --- local: preinit disabled on this 4.9 tree --------------------------------
# Upstream compiles the freestanding runner with $(CC) but links it with $(LD).
# Here the kernel is built with clang while $(LD) comes from the GCC cross
# toolchain, so the runner object is emitted for the host (x86-64) and ld aborts
# with "Relocations in generic ELF (EM: 62)".  runner.elf is therefore never
# produced and preinit/blob.S fails its .incbin on the missing file.
# preinit is optional (preinit.h supplies no-op stubs under KSU_NO_PREINIT), so
# disable it instead of papering over the clang/GNU-ld mix; the else branch below
# defines KSU_NO_PREINIT for the rest of the driver.
KSU_PREINIT_ENABLED ?= 0

ifeq ($(KSU_PREINIT_ENABLED),1)
kernelsu-objs += preinit/preinit.o
kernelsu-objs += preinit/blob.o

KSU_RUNNER_OBJ := $(abspath $(obj))
KSU_RUNNER_BIN := $(KSU_RUNNER_OBJ)/preinit/runner.elf
KSU_RUNNER_TARGET := $(obj)/preinit/runner.elf
KSU_RUNNER_SRC := $(KSU_SRC)/preinit/runner

KSU_RUNNER_CFLAGS := $(CLANG_FLAGS) -std=gnu11 -Os -Wall \
	-nostdinc -isystem $(shell $(CC) -print-file-name=include) \
	-I$(KSU_RUNNER_SRC)/nolibc -I$(KSU_UAPI_INC) \
	-I$(KSU_SRC)/include \
	-ffreestanding -fno-stack-protector -fno-pic -fno-pie \
	-fno-asynchronous-unwind-tables -fno-unwind-tables \
	-ffunction-sections -fdata-sections

KSU_RUNNER_LIBS :=
ifeq ($(ARCH),arm)
KSU_RUNNER_LIBS := $(shell $(CC) $(CLANG_FLAGS) -print-libgcc-file-name)
endif

quiet_cmd_ksu_runner = CC      $@
      cmd_ksu_runner = mkdir -p $(dir $@) && \
		$(CC) $(KSU_RUNNER_CFLAGS) -c -o $@.o $< && \
		$(LD) -static -nostdlib -e _start --gc-sections -s -o $@ $@.o $(KSU_RUNNER_LIBS) && \
		rm -f $@.o

$(KSU_RUNNER_TARGET): $(KSU_RUNNER_SRC)/runner.c \
		$(wildcard $(KSU_RUNNER_SRC)/nolibc/*.h \
			   $(KSU_RUNNER_SRC)/nolibc/sys/*.h \
			   $(KSU_SRC)/include/uapi/*.h) FORCE
	$(call if_changed,ksu_runner)

targets += preinit/runner.elf
$(obj)/preinit/blob.o: $(KSU_RUNNER_TARGET)
asflags-y += -DKSU_PREINIT_RUNNER_BIN=\"$(KSU_RUNNER_BIN)\"
else
$(warning -- $(REPO_NAME): preinit disabled (KSU_PREINIT_ENABLED=0))
ccflags-y += -DKSU_NO_PREINIT
endif

