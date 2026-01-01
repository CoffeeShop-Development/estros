SRCS=asm.c dis.c sim.c
OBJS=asm.o dis.o sim.o
PROGS=xm_asm xm_dis xm_sim
SAMPLES_DIR=./samples

all: $(PROGS)

build: $(PROGS)

test: build
	./xm_asm $(SAMPLES_DIR)/add.S $(SAMPLES_DIR)/add.o
	./xm_dis <$(SAMPLES_DIR)/add.o
	./xm_sim $(SAMPLES_DIR)/add.o -t0 -ra -test -quiet

	./xm_asm $(SAMPLES_DIR)/sum.S $(SAMPLES_DIR)/sum.o
	./xm_dis <$(SAMPLES_DIR)/sum.o
	./xm_sim $(SAMPLES_DIR)/sum.o -t0 -ra -test -quiet

	./xm_asm $(SAMPLES_DIR)/memop.S $(SAMPLES_DIR)/memop.o
	./xm_dis <$(SAMPLES_DIR)/memop.o
	./xm_sim $(SAMPLES_DIR)/memop.o -t0 -ra -test -quiet

	./xm_asm $(SAMPLES_DIR)/branch0.S $(SAMPLES_DIR)/branch0.o
	./xm_dis <$(SAMPLES_DIR)/branch0.o
	./xm_sim $(SAMPLES_DIR)/branch0.o -t0 -ra -test -quiet

	./xm_asm $(SAMPLES_DIR)/bcond.S $(SAMPLES_DIR)/bcond.o
	./xm_dis <$(SAMPLES_DIR)/bcond.o
	./xm_sim $(SAMPLES_DIR)/bcond.o -t0 -ra -test -quiet

	./xm_asm $(SAMPLES_DIR)/memcpy.S $(SAMPLES_DIR)/memcpy.o
	./xm_dis <$(SAMPLES_DIR)/memcpy.o
	./xm_sim $(SAMPLES_DIR)/memcpy.o -a0 4096 -a1 8192 -a2 1

clean:
	-rm *.o $(PROGS)

.PHONY: all build test clean

xm_asm: asm.o
	$(CC) $(CFLAGS) $^ -o $@

xm_dis: dis.o
	$(CC) $(CFLAGS) $^ -o $@

xm_sim: sim.o
	$(CC) $(CFLAGS) $^ -o $@ -lm

.o: .c
	$(CC) $(CFLAGS) -c $< -o $@
