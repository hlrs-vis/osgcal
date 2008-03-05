TOPDIR = ../..
include $(TOPDIR)/Make/makedefs 

CXXFILES =\
	osgCalNMExport.cpp\

INC      += -I/usr/local/OpenSceneGraph/include -I/usr/local/include

LIBS     += -L/usr/local/OpenSceneGraph/lib -L/usr/local/lib -losgDB -losg -lIL -lILU -lILUT

INSTFILES = \
	$(CXXFILES)\
	GNUmakefile.inst=GNUmakefile

EXEC = osgCalNMExport

INC +=  $(X_INC) 

include $(TOPDIR)/Make/makerules 

