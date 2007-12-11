TOPDIR = ../..
include $(TOPDIR)/Make/makedefs 

CXXFILES =\
	osgCalPreparer.cpp\

INC      += -I/usr/local/OpenSceneGraph/include -I/usr/local/include
LIBS     += -L/usr/local/OpenSceneGraph/lib -L/usr/local/lib -lcal3d -losg -losgDB -L$(ROOTDIR)/bin -losgCal

INSTFILES = \
	$(CXXFILES)\
	GNUmakefile.inst=GNUmakefile

EXEC = osgCalPreparer

INC +=  $(X_INC) 

include $(TOPDIR)/Make/makerules 

