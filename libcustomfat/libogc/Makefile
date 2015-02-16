#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITPPC)),)
$(error "Please set DEVKITPPC in your environment. export DEVKITPPC=<path to>devkitPPC)
endif

ifeq ($(PLATFORM),wii)
include $(DEVKITPPC)/wii_rules
endif

ifeq ($(PLATFORM),cube)
include $(DEVKITPPC)/gamecube_rules
endif

#---------------------------------------------------------------------------------
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# INCLUDES is a list of directories containing extra header files
# DATA is a list of directories containing binary files
# LIBDIR is where the built library will be placed
# all directories are relative to this makefile
#---------------------------------------------------------------------------------
BUILD		?=	wii_release
SOURCES		:=	../source
INCLUDES	:=	../include
DATA		:=
LIBDIR		:=	$(TOPDIR)/libogc/lib

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
CFLAGS	= -g -Os -Wall $(MACHDEP) $(INCLUDE)
CXXFLAGS	=	$(CFLAGS)

ASFLAGS	:=	-g

ifneq ($(BUILD),debug)
export CUBEBIN	:=	$(LIBDIR)/$(PLATFORM)/libfat.a
else
export CUBEBIN	:=	$(LIBDIR)/$(PLATFORM)/libfatd.a
CFLAGS	+=	-DFAT_DEBUG
endif

#---------------------------------------------------------------------------------
# any extra libraries we wish to link with the project
#---------------------------------------------------------------------------------
LIBS	:=

#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level containing
# include and lib
#---------------------------------------------------------------------------------
LIBDIRS	:=

#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export TOPDIR ?= $(CURDIR)/..


export DEPSDIR := $(CURDIR)/$(BUILD)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
			$(foreach dir,$(DATA),$(CURDIR)/$(dir))

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))


export OFILES	:=	$(addsuffix .o,$(BINFILES)) \
			$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD) \
			-I$(LIBOGC_INC)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib) \
										-L$(LIBOGC_LIB)

.PHONY: $(BUILD) clean

#---------------------------------------------------------------------------------
$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr wii_debug wii_release cube_debug cube_release $(LIBDIR) include

all: $(CUBEBIN)

dist-bin:
	@mkdir -p include
	@cp $(TOPDIR)/include/fat.h $(TOPDIR)/include/libfatversion.h include
	@tar --exclude=.svn --exclude=*CVS* -cvjf $(TOPDIR)/distribute/$(VERSTRING)/libfat-ogc-$(VERSTRING).tar.bz2 include lib

install:
	@cp lib/wii/libfat.a $(DEVKITPRO)/libogc/lib/wii
	@cp lib/cube/libfat.a $(DEVKITPRO)/libogc/lib/cube
	@cp $(TOPDIR)/include/fat.h $(TOPDIR)/include/libfatversion.h $(DEVKITPRO)/libogc/include

#---------------------------------------------------------------------------------
else

DEPENDS	:=	$(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
$(CUBEBIN)	:	$(OFILES) $(LIBDIR)/$(PLATFORM)
	@rm -f "$(CUBEBIN)"
	@$(AR) rcs "$(CUBEBIN)" $(OFILES)
	@echo built ... $(notdir $@)

$(LIBDIR)/$(PLATFORM):
	mkdir  -p $(LIBDIR)/$(PLATFORM)

-include $(DEPENDS)

#---------------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------------

