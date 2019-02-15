TARGET		:= bin2h
PLATFORM	:= none
CFLAGS		:= 

# The Windows binary tracked in this repo seems alright, so just skip building
# for Windows users. Otherwise, force MacOS/Linux users to compile their own 
# version to keep around locally. The UNIX binary should *not* be tracked in 
# this repository.

ifeq ($(OS),Windows_NT)
	#TARGET = bin2h.exe
	#CFLAGS = -s -Os -static
	PLATFORM = windows
else
	UNAME := $(shell uname -s)
	ifeq ($(UNAME),Darwin)
		CFLAGS = -s -Os
		PLATFORM = mac
	endif

	ifeq ($(UNAME),Linux)
		CFLAGS = -m32 -s -Os -static
		PLATFORM = linux
	endif
endif

ifeq ($(PLATFORM),none)
	$(error Couldnt detect platform - failed to compile bin2h)
endif

# -----------------------------------------------------------------------------
# Targets

.PHONY: all clean
all: $(TARGET)
$(TARGET): main.c
	ifeq ($(OS),windows)
		@echo "Using prebuilt bin2h.exe ..."
	else
		@echo "Building bin2h ..."
		gcc $< $(CFLAGS) -o $@
	endif
clean:
	@echo "cleaning bin2h ..."
	@rm -f $(TARGET)
