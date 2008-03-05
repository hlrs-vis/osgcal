default: wrapper
	make default -f GNUmakefile.compile

debug: wrapper 
	make debug -f GNUmakefile.compile

all: wrapper
	make -f GNUmakefile.compile

install:
	make install -f GNUmakefile.compile

clean:
	make clean -f GNUmakefile.compile

wrapper:
	cd ../../wrapper && make && cd ../src/osgWrappers

# ../../wrapper/Makefile : 
# all:
# #ifeq ($(OS),MINGW)
# 	cd ../doc/doxygen && doxygen && cd ../../wrapper
# #	genwrapper/genwrapper.exe -c ./genwrapper/genwrapper.orig -d ../ | doxygen -
# 	genwrapper/genwrapper.exe -c ./genwrapper/genwrapper.orig ../doc/doxygen ../
# #endif
