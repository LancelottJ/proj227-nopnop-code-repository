# FIX ME
TREE=/usr/src/linux-5.14

BPFTOOL=$(TREE)/tools/bpf/bpftool/bpftool
LIBBPF=$(TREE)/tools/lib/bpf/libbpf.a
INCLUDES := -I$(TREE)/tools/include/uapi -I$(TREE)/tools/lib/ -I$(TREE)/tools/bpf/bpftool/ -I.
ARCH := $(shell uname -m | sed 's/x86_64/x86/')

all: exitsnoop

exitsnoop: exitsnoop.c exitsnoop.skel.h
	gcc -Wall -g -Wno-unused-variable $(INCLUDES) $< -o $@ $(LIBBPF) -lelf -lz

%.bpf.o: %.bpf.c
	clang -g -O2 -target bpf -D__TARGET_ARCH_$(ARCH) $(INCLUDES) -o $@ -c $^
#   llvm-strip -g $@

%.skel.h: %.bpf.o
	$(BPFTOOL) gen skeleton $< > $@

clean:
	rm -f *.o exitsnoop *.skel.h
