TOPDIR = ../..
include $(TOPDIR)/Make/makedefs 

CXXFILES =\
	osgCalViewer.cpp\

INC      += -I/usr/local/OpenSceneGraph/include -I/usr/local/include

LIBS     += -L/usr/local/OpenSceneGraph/lib -L/usr/local/lib -lcal3d -losg -losgDB -losgGA -losgUtil -losgViewer -L$(ROOTDIR)/bin -losgCal

INSTFILES = \
	$(CXXFILES)\
	GNUmakefile.inst=GNUmakefile

EXEC = osgCalViewer

INC +=  $(X_INC) 

include $(TOPDIR)/Make/makerules 

