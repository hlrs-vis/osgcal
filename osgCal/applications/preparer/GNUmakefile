TOPDIR = ../..
include $(TOPDIR)/Make/makedefs 

CXXFILES =\
	osgCalPreparer.cpp\

LIBS     += -lcal3d -losgDB -L$(ROOTDIR)/bin -losgCal

INSTFILES = \
	$(CXXFILES)\
	GNUmakefile.inst=GNUmakefile

EXEC = osgCalPreparer

INC +=  $(X_INC) 

include $(TOPDIR)/Make/makerules 

