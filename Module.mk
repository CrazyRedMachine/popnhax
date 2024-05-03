cflags          += -DWIN32_LEAN_AND_MEAN -DCOBJMACROS -Ipkcs11 -Wno-attributes

avsvers_32      := 1700 1508
avsvers_64      := 1700 1509

imps            += avs avs-ea3

include util/Module.mk
include minhook/Module.mk
include popnhax/Module.mk

#
# Distribution build rules
#

zipdir          := $(BUILDDIR)/zip
popnhax_version := $(shell grep "define PROGRAM_VERSION" popnhax/dllmain.cc | cut -d'"' -f2)

$(BUILDDIR)/popnhax_v$(popnhax_version).zip: \
	build/bin/avs2_1508-32/popnhax.dll
	@echo ... $@
	@mkdir -p $(zipdir)
	@cp -a -p build/bin/avs2_1508-32/popnhax.dll $(zipdir)
	@cp -r -a -p dist/popnhax/* $(zipdir)
	@cd $(zipdir) \
	&& zip -r ../popnhax_v$(popnhax_version).zip ./*

all: $(BUILDDIR)/popnhax_v$(popnhax_version).zip
