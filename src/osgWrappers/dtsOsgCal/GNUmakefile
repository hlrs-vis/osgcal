TOPDIR = ../../..
include $(TOPDIR)/Make/makedefs 

CXXFILES =\
	CoreModel.cpp\
	Model.cpp\
	IOUtils.cpp\
	SubMeshHardware.cpp\
	SubMeshSoftware.cpp\

LIBS   += -losg -losgIntrospection  $(GL_LIBS) $(OTHER_LIBS) -ldtsOsgCal
INC    += -I ../../../include

OPTF = -O

TARGET_BASENAME = dtsOsgCal
include $(TOPDIR)/Make/cygwin_wrapper_def
WRAPPER = $(WRAPPER_PREFIX)$(TARGET_BASENAME).$(PLUGIN_EXT)

include $(TOPDIR)/Make/makerules 
