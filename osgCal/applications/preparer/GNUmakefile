TOPDIR = ../..
include $(TOPDIR)/Make/makedefs 

CXXFILES =\
	osgCalPreparer.cpp\

INC      += -I/usr/local/OpenSceneGraph/include
LIBS     += -L/usr/local/OpenSceneGraph/lib -lcal3d -losgDB -L$(ROOTDIR)/bin -losgCal

INSTFILES = \
	$(CXXFILES)\
	GNUmakefile.inst=GNUmakefile

EXEC = osgCalPreparer

INC +=  $(X_INC) 

include $(TOPDIR)/Make/makerules 

