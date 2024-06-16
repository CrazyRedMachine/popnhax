avsdlls            += popnhax

deplibs_popnhax    := \
    avs \

ldflags_popnhax    := \
	-lwinmm -lpsapi

libs_popnhax       := \
	util \
	minhook \
	libdisasm

srcpp_popnhax      := \
    dllmain.cc \
    config.cc \
    loader.cc \
    SearchFile.cc \
    translation.cc \
    omnimix_patch.cc \
    custom_categs.cc
