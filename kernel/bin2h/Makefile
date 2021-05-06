Target :=
ifeq ($(OS),Windows_NT)
	Target := bin2h.exe
else
	ifeq ($(shell grep Microsoft /proc/version), $(shell cat /proc/version))
		Target := bin2h_wsl
	else
		Target := bin2h
	endif
endif

.PHONY: all forced clean bin2h bin2h_wsl bin2h.exe

all: $(Target)
forced: clean all

bin2h:
	@bash make.sh
	
bin2h_wsl:
	@./make.cmd
	@mv bin2h.exe bin2h

bin2h.exe:
	@make.cmd

clean:
	@rm -fr bin2h
	@rm -fr bin2h.exe