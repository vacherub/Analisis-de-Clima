CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra -std=gnu11
UNAME_S := $(shell uname -s 2>/dev/null || echo Windows)

TARGET  := forecast
EXEEXT  :=
OBJS    := forecast.o cJSON.o

# ---- Detección de plataforma ----
# Windows (MSVC/MinGW/MSYS2): WinHTTP integrado, sin dependencias externas.
# Linux / macOS: libcurl (en macOS ya viene incluido en el sistema).
ifeq ($(OS),Windows_NT)
  EXEEXT  := .exe
  LDLIBS  += -lwinhttp
else ifneq ($(findstring MINGW,$(UNAME_S)),)
  EXEEXT  := .exe
  LDLIBS  += -lwinhttp
else ifneq ($(findstring MSYS,$(UNAME_S)),)
  EXEEXT  := .exe
  LDLIBS  += -lwinhttp
else ifneq ($(findstring Darwin,$(UNAME_S)),)
  LDLIBS  += -lcurl -lm
else
  LDLIBS  += -lcurl -lm
endif

TARGET  := $(TARGET)$(EXEEXT)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDLIBS)

%.o: %.c cJSON.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
