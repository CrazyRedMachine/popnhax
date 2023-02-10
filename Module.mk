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

$(zipdir)/:
	mkdir -p $@

$(BUILDDIR)/popnhax.zip: \
		build/bin/avs2_1508-32/popnhax.dll \
		dist/popnhax/popnhax.xml \
		| $(zipdir)/
	echo ... $@
	zip -j $@ $^

$(BUILDDIR)/bemanihax.zip: \
		$(BUILDDIR)/popnhax.zip \
		| $(zipdir)/
	echo ... $@
	zip -9 -q -j $@ $^

all: $(BUILDDIR)/bemanihax.zip
