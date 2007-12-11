TOPDIR = ../../..
include $(TOPDIR)/Make/makedefs 

CXXFILES =\
	CoreModel.cpp\
	Model.cpp\
	IOUtils.cpp\
	SubMeshHardware.cpp\
	SubMeshSoftware.cpp\

LIBS   += -L/usr/local/OpenSceneGraph/lib -L/usr/local/lib \
	-losg -losgIntrospection  $(GL_LIBS) $(OTHER_LIBS) -losgCal
INC    += -I ../../../include -I/usr/local/OpenSceneGraph/include -I/usr/local/include


OPTF = -O

TARGET_BASENAME = osgCal
include $(TOPDIR)/Make/cygwin_wrapper_def
WRAPPER = $(WRAPPER_PREFIX)$(TARGET_BASENAME).$(PLUGIN_EXT)

include $(TOPDIR)/Make/makerules 
