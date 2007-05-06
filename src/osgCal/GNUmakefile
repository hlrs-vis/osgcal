TOPDIR = ../..
include $(TOPDIR)/Make/makedefs 

CXXFILES =\
	CoreModel.cpp \
	IOUtils.cpp \
	Model.cpp \
	SubMeshHardware.cpp \
	SubMeshSoftware.cpp

#CXXFLAGS=-march=k8 -ftree-vectorize -O2  -I$(ROOTDIR)/include -DOSGCAL_LIBRARY #-fopenmp
#   ^ interesing, but openmp gives speedup only with these options
#    CXXFLAGS=-march=k8 -m3dnow -mmmx -msse2 -msse3 -ftree-vectorize  -O2 -I$(ROOTDIR)/include -DOSGCAL_LIBRARY 
#   -march=k8      -- 1.5->1.35
#   -march=k8 -ftree-vectorize -- 1.35 -> 1.15
#   -ftree-vectorize -- slower 1.5s->1.7s (no improvements w/o march)
#   -msse2 -msse3    -- slower - back to 1.35
#   -m3dnow -mmmx    -- no difference
#    -ffast-math     -- no difference + unstable FPS

INC     += -I/usr/local/OpenSceneGraph/include

DEF     += -DOSGCAL_LIBRARY

LIBS    += -L/usr/local/OpenSceneGraph/lib $(GL_LIBS) $(OTHER_LIBS) -lcal3d -losg -losgDB

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
