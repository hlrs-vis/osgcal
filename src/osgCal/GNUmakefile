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
	shaders/SkeletalFragmentShader.h \
	shaders/SkeletalVertexShaderDepthOnly.h \
	shaders/SkeletalFragmentShaderDepthOnly.h

shaders/SkeletalVertexShader.h: SkeletalVertexShader.glsl
ifeq ($(OS),MINGW)
	sed -f ../glsl2cpp.sed <../SkeletalVertexShader.glsl >../shaders/SkeletalVertexShader.h
else
	sed -e s/\\r// <../SkeletalVertexShader.glsl | sed -f ../glsl2cpp.sed >../shaders/SkeletalVertexShader.h
endif

shaders/SkeletalFragmentShader.h: SkeletalFragmentShader.glsl
ifeq ($(OS),MINGW)
	sed -f ../glsl2cpp.sed <../SkeletalFragmentShader.glsl >../shaders/SkeletalFragmentShader.h
else
	sed -e s/\\r// <../SkeletalFragmentShader.glsl | sed -f ../glsl2cpp.sed >../shaders/SkeletalFragmentShader.h
endif

shaders/SkeletalVertexShaderDepthOnly.h: SkeletalVertexShaderDepthOnly.glsl
ifeq ($(OS),MINGW)
	sed -f ../glsl2cpp.sed <../SkeletalVertexShaderDepthOnly.glsl >../shaders/SkeletalVertexShaderDepthOnly.h
else
	sed -e s/\\r// <../SkeletalVertexShaderDepthOnly.glsl | sed -f ../glsl2cpp.sed >../shaders/SkeletalVertexShaderDepthOnly.h
endif

shaders/SkeletalFragmentShaderDepthOnly.h: SkeletalFragmentShaderDepthOnly.glsl
ifeq ($(OS),MINGW)
	sed -f ../glsl2cpp.sed <../SkeletalFragmentShaderDepthOnly.glsl >../shaders/SkeletalFragmentShaderDepthOnly.h
else
	sed -e s/\\r// <../SkeletalFragmentShaderDepthOnly.glsl | sed -f ../glsl2cpp.sed >../shaders/SkeletalFragmentShaderDepthOnly.h
endif
