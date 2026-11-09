
NVCC := nvcc

TARGET := delta-stepping
SRC := delta-stepping-baseline.cu

ARCH ?= sm_120

NVCCFLAGS := -O2 -std=c++17 -arch=$(ARCH) -lineinfo

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(NVCC) $(NVCCFLAGS) $^ -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
