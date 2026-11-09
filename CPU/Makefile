CC = gcc
CFLAGS = -O3
DADOS = ../dados/gerador/v30e120

.PHONY: clean CPU_RUN_V0

BUILD_V0: ./CPU_delta_v0.c
	$(CC) $(CFLAGS) ./CPU_delta_v0.c -o ./CPU_delta_v0

RUN_V0: BUILD_V0
	./CPU_delta_v0 $(DADOS)

clean:
	rm -f ./CPU_delta_v0
