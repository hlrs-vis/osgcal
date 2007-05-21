TOPDIR = ../..
include $(TOPDIR)/Make/makedefs 

CXXFILES =\
	CoreModel.cpp \
	IOUtils.cpp \
	Model.cpp \
	SubMeshHardware.cpp \
	SubMeshSoftware.cpp

#OPTF='-march=k8 -ftree-vectorize -O2' # -fopenmp

INC     += -I/usr/local/OpenSceneGraph/include -I/usr/local/include

DEF     += -DOSGCAL_LIBRARY

LIBS    += -L/usr/local/OpenSceneGraph/lib -L/usr/local/lib $(GL_LIBS) $(OTHER_LIBS) -lcal3d -losg -losgDB

TARGET_BASENAME = osgCal
LIB = $(LIB_PREFIX)$(TARGET_BASENAME).$(LIB_EXT)

include $(TOPDIR)/Make/makerules 

CoreModel.cpp: \
		shaders/SkeletalVertexShader.h \
		shaders/SkeletalFragmentShader.h

shaders/SkeletalVertexShader.h: SkeletalVertexShader.glsl
		dos2unix <../SkeletalVertexShader.glsl | sed -f ../glsl2cpp.sed >../shaders/SkeletalVertexShader.h

shaders/SkeletalFragmentShader.h: SkeletalFragmentShader.glsl
		dos2unix <../SkeletalFragmentShader.glsl | sed -f ../glsl2cpp.sed >../shaders/SkeletalFragmentShader.h
