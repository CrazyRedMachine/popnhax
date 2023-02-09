# vim: noexpandtab sts=8 sw=8 ts=8

#
# Overridable variables
#

export SHELL    := /bin/bash
BUILDDIR        ?= build

#
# Internal variables
#

export TZ        := /usr/share/zoneinfo/Japan

ifeq ($(USE_CCACHE),yes)
ccache          := ccache
else
ccache          :=
endif

depdir          := $(BUILDDIR)/dep
objdir          := $(BUILDDIR)/obj
bindir          := $(BUILDDIR)/bin

toolchain_32    := i686-w64-mingw32-
toolchain_64    := x86_64-w64-mingw32-

cppflags	:= -I. \
    -DNDEBUG \
    -D_NEED_FULLVERSION_INFO=1 -D_SECURE_SCL=1 \
    -D_CRT_SECURE_NO_WARNINGS=1 -D_NO_CRT_STDIO_INLINE=1 \
    -D_WINSOCK_DEPRECATED_NO_WARNINGS=1 \
    -DFKG_FORCED_USAGE=1 -DOFFICIAL_BUILD=1 -DBETA=1 -DDEVL=1

com_wflags  := -Wall -Werror -Wpointer-arith -Wreturn-type \
    -Wwrite-strings -Wswitch -Wcast-align -Wchar-subscripts \
    -Wredundant-decls -Wunreachable-code -Wno-pedantic \
    -Wshadow -Winline -Wno-cast-qual -Wno-multichar \
    -fstrict-aliasing -Wno-unused-function \
    -Warray-bounds=2 -Wno-redundant-decls

wflags      := $(com_wflags) -Wno-strict-prototypes -Wnested-externs  \
               -Wno-discarded-qualifiers
wxxflags    := $(com_wflags) -Wno-old-style-cast

cflags      := -O3 -pipe -ffunction-sections -fdata-sections \
               -std=gnu11 $(wflags)
cxxflags    := -O3 -pipe -ffunction-sections -fdata-sections \
               -std=c++11 $(wxxflags)

ldflags		:= -Wl,--gc-sections -static -static-libgcc -lstdc++ \
               -fdiagnostics-color -Werror \
               -Wl,--gc-keep-exported \
               -Wl,--enable-auto-image-base \
               -Wl,--exclude-all-symbols \
               -Wl,--dynamicbase \
               -Wl,--nxcompat \
               -Wl,-s

#
# The first target that GNU Make encounters becomes the default target.
# Define our ultimate target (`all') here, and also some helpers
#

all:

.PHONY: clean

clean:
	rm -rf $(BUILDDIR)

#
# Pull in module definitions
#

deps		:=

dlls		:=
exes		:=
imps		:=
libs		:=

avsdlls		:=
avsexes		:=

include Module.mk

modules		:= $(dlls) $(exes) $(libs) $(avsdlls) $(avsexes)

#
# $1: Bitness
# $2: AVS2 minor version
# $3: Module
#

optflags_64 +=  -mfpmath=sse -march=x86-64 \
                -mtune=generic -mabi=ms -malign-data=cacheline \
                -minline-stringops-dynamically -funswitch-loops \
                -funroll-loops -fschedule-insns2 -fsched-pressure \
                -fprefetch-loop-arrays --param prefetch-latency=300 \
                -fsel-sched-pipelining -fselective-scheduling \
                -ftree-vectorize -fbranch-target-load-optimize \
                -flive-range-shrinkage -falign-functions=16 \
                -flto -fno-use-linker-plugin -masm=intel

optflags_32 +=  -mfpmath=sse -march=pentium-m -mtune=generic \
                -mabi=ms -malign-data=cacheline \
                -minline-stringops-dynamically -funswitch-loops \
                -funroll-loops -fschedule-insns2 -fsched-pressure \
                -fprefetch-loop-arrays --param prefetch-latency=300 \
                -fsel-sched-pipelining -fselective-scheduling \
                -ftree-vectorize -fbranch-target-load-optimize \
                -flive-range-shrinkage -falign-functions=16 \
                -flto -fno-use-linker-plugin -masm=intel

cflags_32   :=  $(optflags_32)
cxxflags_32 :=  $(optflags_32)
cflags_64   :=  $(optflags_64)
cxxflags_64 :=  $(optflags_64)

define t_moddefs

cppflags_$3	+= $(cppflags) -DBUILD_MODULE=$3
cflags_$3	+= $(cflags)
cxxflags_$3	+= $(cxxflags)
ldflags_$3	+= $(ldflags)
srcdir_$3	:= $3

endef

$(eval $(foreach module,$(modules),$(call t_moddefs,_,_,$(module))))

##############################################################################

define t_bitness

subdir_$1_indep	:= indep-$1
bindir_$1_indep	:= $(bindir)/$$(subdir_$1_indep)

$$(bindir_$1_indep):
	mkdir -p $$@

$$(eval $$(foreach imp,$(imps),$$(call t_import,$1,indep,$$(imp))))
$$(eval $$(foreach dll,$(dlls),$$(call t_linkdll,$1,indep,$$(dll))))
$$(eval $$(foreach exe,$(exes),$$(call t_linkexe,$1,indep,$$(exe))))
$$(eval $$(foreach lib,$(libs),$$(call t_archive,$1,indep,$$(lib))))

$$(eval $$(foreach avsver,$$(avsvers_$1),$$(call t_avsver,$1,$$(avsver))))

endef

##############################################################################

define t_avsver

subdir_$1_$2	:= avs2_$2-$1
bindir_$1_$2	:= $(bindir)/$$(subdir_$1_$2)

$$(bindir_$1_$2):
	mkdir -p $$@

$$(eval $$(foreach imp,$(imps),$$(call t_import,$1,$2,$$(imp))))
$$(eval $$(foreach dll,$(avsdlls),$$(call t_linkdll,$1,$2,$$(dll))))
$$(eval $$(foreach exe,$(avsexes),$$(call t_linkexe,$1,$2,$$(exe))))

endef

##############################################################################

define t_compile

depdir_$1_$2_$3	:= $(depdir)/$$(subdir_$1_$2)/$3
abslib_$1_$2_$3	:= $$(libs_$3:%=$$(bindir_$1_indep)/lib%.a)
absdpl_$1_$2_$3	:= $$(deplibs_$3:%=$$(bindir_$1_$2)/lib%.a)
objdir_$1_$2_$3	:= $(objdir)/$$(subdir_$1_$2)/$3
obj_$1_$2_$3	:=	$$(src_$3:%.c=$$(objdir_$1_$2_$3)/%.o) \
			$$(rc_$3:%.rc=$$(objdir_$1_$2_$3)/%_rc.o) \
			$$(srcpp_$3:%.cc=$$(objdir_$1_$2_$3)/%.o)

deps		+= $$(src_$3:%.c=$$(depdir_$1_$2_$3)/%.d) \
	$$(srcpp_$3:%.cc=$$(depdir_$1_$2_$3)/%.d)

$$(depdir_$1_$2_$3):
	mkdir -p $$@

$$(objdir_$1_$2_$3):
	mkdir -p $$@

$$(objdir_$1_$2_$3)/%.o: $$(srcdir_$3)/%.c \
		| $$(depdir_$1_$2_$3) $$(objdir_$1_$2_$3)
	$(ccache) $$(toolchain_$1)gcc $$(cflags_$3) $$(cflags_$1) $$(cppflags_$3) \
		-MMD -MF $$(depdir_$1_$2_$3)/$$*.d -MT $$@ -MP \
		-DAVS_VERSION=$2 -c -o $$@ $$<

$$(objdir_$1_$2_$3)/%.o: $$(srcdir_$3)/%.cc \
		| $$(depdir_$1_$2_$3) $$(objdir_$1_$2_$3)
	$(ccache) $$(toolchain_$1)g++ $$(cxxflags_$3) $$(cxxflags_$1) $$(cppflags_$3) \
		-MMD -MF $$(depdir_$1_$2_$3)/$$*.d -MT $$@ -MP \
		-DAVS_VERSION=$2 -c -o $$@ $$<

$$(objdir_$1_$2_$3)/%_rc.o: $$(srcdir_$3)/%.rc \
		| $$(depdir_$1_$2_$3) $$(objdir_$1_$2_$3)
	$$(toolchain_$1)windres $$(cppflags_$3) $$< $$@

endef

##############################################################################

define t_archive

$(t_compile)

$$(bindir_$1_$2)/lib$3.a: $$(obj_$1_$2_$3) | $$(bindir_$1_$2)
	$$(toolchain_$1)gcc-ar r $$@ $$^ 2> /dev/null
	$$(toolchain_$1)gcc-ranlib $$@

endef

##############################################################################

define t_linkdll

$(t_compile)

dll_$1_$2_$3	:= $$(bindir_$1_$2)/$3.dll
implib_$1_$2_$3	:= $$(bindir_$1_$2)/lib$3.a

$$(dll_$1_$2_$3) $$(implib_$1_$2_$3):	$$(obj_$1_$2_$3) $$(abslib_$1_$2_$3) \
					$$(absdpl_$1_$2_$3) \
					$$(srcdir_$3)/$3.def | $$(bindir_$1_$2)
	$(ccache) $$(toolchain_$1)gcc -shared $$(srcdir_$3)/$3.def \
		-o $$(dll_$1_$2_$3) -Wl,--out-implib,$$(implib_$1_$2_$3) \
		$$^ $$(ldflags_$3) $(optflags_$1)
	strip -s $$(dll_$1_$2_$3)
	ranlib $$(implib_$1_$2_$3)

endef

##############################################################################

define t_linkexe

$(t_compile)

exe_$1_$2_$3	:= $$(bindir_$1_$2)/$3.exe

$$(exe_$1_$2_$3): $$(obj_$1_$2_$3) $$(abslib_$1_$2_$3) $$(absdpl_$1_$2_$3) \
		| $$(bindir_$1_$2)
	$(ccache) $$(toolchain_$1)gcc -o $$@ $$^ $$(ldflags_$3) $(optflags_$1)
	strip -s $$@

endef

##############################################################################

define t_import

impdef_$1_$2_$3	?= imports/import_$1_$2_$3.def

$$(bindir_$1_$2)/lib$3.a: $$(impdef_$1_$2_$3) | $$(bindir_$1_$2)
	dlltool -l $$@ -d $$<

endef

##############################################################################

$(eval $(foreach bitness,32 64,$(call t_bitness,$(bitness))))

#
# Pull in GCC-generated dependency files
#

-include $(deps)

