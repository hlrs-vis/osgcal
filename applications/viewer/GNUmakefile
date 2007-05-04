TOPDIR = ../..
include $(TOPDIR)/Make/makedefs 

CXXFILES =\
	osgCalViewer.cpp\

LIBS     += -lcal3d -losg -losgViewer -losgDB -L$(ROOTDIR)/bin -losgCal -losgGA

INSTFILES = \
	$(CXXFILES)\
	GNUmakefile.inst=GNUmakefile

EXEC = osgCalViewer

INC +=  $(X_INC) 

include $(TOPDIR)/Make/makerules 

