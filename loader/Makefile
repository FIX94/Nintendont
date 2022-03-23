#---------------------------------------------------------------------------------
# Clear the implicit built in rules
#---------------------------------------------------------------------------------

.SUFFIXES:
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITPPC)),)
$(error "Please set DEVKITPPC in your environment. export DEVKITPPC=<path to>devkitPPC")
endif

include $(DEVKITPPC)/wii_rules

#---------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# INCLUDES is a list of directories containing extra header files
# ARTIFACTS is to package a zip for the user to unpack to their SD card/HDD
#---------------------------------------------------------------------------------
TARGET		:= $(notdir $(CURDIR))
BUILD		:= build
SOURCES		:= source
SOURCES		+= source/unzip
DATA		:= data
INCLUDES	:= include ../fatfs
ARTIFACTS	:= ../build/apps/Nintendont
NINTENTDONTVERSION_H := ../common/include/NintendontVersion.h

#---------------------------------------------------------------------------------
# metadata
#---------------------------------------------------------------------------------
# If dol does not come from FIX94 we identify source on the screen
NIN_SPECIAL_VERSION	:=	$(if $(and $(GITHUB_REPOSITORY_OWNER),$(findstring FIX94,$(GITHUB_REPOSITORY_OWNER))),,\"-$(GITHUB_REPOSITORY_OWNER)\")
# As preprocessor defines, DATETIME is unquoted, META_DATETIME is quoted
DATETIME			:=	$(shell git show -s --format=%cI | grep -o -E "[[:digit:]]*" - | head -n 6 | paste -sd '')

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------

ASFLAGS := $(MACHDEP) -mregnames -D_LANGUAGE_ASSEMBLY
CFLAGS	:= -O3 -g -std=gnu89 -Wno-format-truncation \
	   -Wall $(MACHDEP) $(INCLUDE) -DDEBUG -DDEBUG_MODULE_PATCH -DARCH_IS_BIG_ENDIAN \
	   -DNIN_SPECIAL_VERSION=$(NIN_SPECIAL_VERSION) \
	   -DMETA_DATETIME=\"$(DATETIME)\"
CXXFLAGS := $(CFLAGS)

# NOTE: Do NOT change to := - needed for proper expansion of $(CURDIR) later.
LDFLAGS	= -g $(MACHDEP) -Wl,-Map,$(notdir $@).map,-wrap,wiiuse_register

#---------------------------------------------------------------------------------
# any extra libraries we wish to link with the project
#---------------------------------------------------------------------------------
LIBS	:= -lfreetype -lpngu -lpng -lz \
	   -lwiiuse -lwiidrc -lwupc -lbte -lfatfs-ppc -logc -ldi -lm

#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level containing
# include and lib
#---------------------------------------------------------------------------------
LIBDIRS	= $(CURDIR)/extlibs
# NOTE: FatFS doesn't use the standard [component]/lib structure,
# so we'll add a -L ourselves.
LDFLAGS	+= -L$(CURDIR)/../../fatfs

#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/$(TARGET)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
					$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

#---------------------------------------------------------------------------------
# automatically build a list of object files for our project
#---------------------------------------------------------------------------------
CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.S)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

#---------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
	export LD	:=	$(CC)
else
	export LD	:=	$(CXX)
endif

export OFILES	:=	$(addsuffix .o,$(BINFILES)) \
					$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) \
					$(sFILES:.s=.o) $(SFILES:.S=.o)

#---------------------------------------------------------------------------------
# build a list of include paths
#---------------------------------------------------------------------------------
export INCLUDE	:=	$(foreach dir,$(INCLUDES), -iquote $(CURDIR)/$(dir)) \
					$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
					-I$(CURDIR)/$(BUILD) -I$(CURDIR)/include \
					-I$(LIBOGC_INC)

#---------------------------------------------------------------------------------
# build a list of library paths
#---------------------------------------------------------------------------------
export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib) \
					-L$(LIBOGC_LIB)

export OUTPUT	:=	$(CURDIR)/$(TARGET)
.PHONY: $(BUILD) clean

#---------------------------------------------------------------------------------
# 1, change version number
# 2, build loader.dol
# 3, output meta.xml using C preprocessor on a template
$(BUILD):
	if [ -n "$(GITHUB_RUN_NUMBER)" ]; then \
		sed -i 's/#define NIN_MINOR_VERSION.*$$/#define NIN_MINOR_VERSION\t\t\t$(GITHUB_RUN_NUMBER)/' $(NINTENTDONTVERSION_H); \
	fi
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
	@[ -d $(ARTIFACTS) ] || mkdir -p $(ARTIFACTS)
	@cp -p $(OUTPUT).dol $(ARTIFACTS)/boot.dol
	$(CC) -E -traditional -P \
		-include $(NINTENTDONTVERSION_H) \
		-DDATETIME=$(DATETIME) \
		../metaxml.sx \
		| sed -n '/^<?xml/,$$p' \
		> $(ARTIFACTS)/meta.xml

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	-$(RM) -rf $(BUILD) $(OUTPUT).elf $(OUTPUT).dol


#---------------------------------------------------------------------------------
run:
	wiiload $(TARGET).dol

#---------------------------------------------------------------------------------
else

DEPENDS	:=	$(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
$(OUTPUT).dol: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)

#---------------------------------------------------------------------------------
# This rule links in binary data with the .jpg extension
#---------------------------------------------------------------------------------
%.bin.o	:	%.bin
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)
	
%.zip.o	:	%.zip
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)
	
%.png.o	:	%.png
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)
	
-include $(DEPENDS)
#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------
