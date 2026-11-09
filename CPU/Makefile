CC = gcc
CFLAGS = -O3 -Wall
DADOS_COMPUTADO = ../dados/computado/v50ke30m.txt
DELTA ?= 0.005

.PHONY: all clean BENCHMARK

all: ./CPU_delta_v0 ./CPU_delta_v1

./CPU_delta_v0: ./CPU_delta_v0.c
	$(CC) $(CFLAGS) ./CPU_delta_v0.c -o ./CPU_delta_v0

./CPU_delta_v1: ./CPU_delta_v1.c
	$(CC) $(CFLAGS) -mavx2 ./CPU_delta_v1.c -o ./CPU_delta_v1

BENCHMARK: all
	@node benchmark.js $(DADOS_COMPUTADO) $(DELTA)

clean:
	rm -f ./CPU_delta_v0 ./CPU_delta_v1