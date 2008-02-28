ADD_DEFINITIONS(-DOSGCAL_LIBRARY)


SET(LIB_NAME osgCal)


SET(LIBRARY_OUTPUT_PATH ${OSGCAL_BINARY_DIR}/lib)

SET(HEADER_PATH ${OSGCAL_INCLUDE_DIR}/${LIB_NAME})
SET(LIB_PUBLIC_HEADERS
    ${HEADER_PATH}/CoreMesh
    ${HEADER_PATH}/DepthMesh
    ${HEADER_PATH}/HardwareMesh
    ${HEADER_PATH}/Mesh
    ${HEADER_PATH}/MeshDisplayLists
    ${HEADER_PATH}/MeshParameters
    ${HEADER_PATH}/Model
    ${HEADER_PATH}/SoftwareMesh
    ${HEADER_PATH}/CoreModel
    ${HEADER_PATH}/Export
    ${HEADER_PATH}/Material
    ${HEADER_PATH}/MeshData
    ${HEADER_PATH}/MeshLoader
    ${HEADER_PATH}/MeshStateSets
    ${HEADER_PATH}/ShadersCache
    ${HEADER_PATH}/StateSetCache
)
#FILE(GLOB h_files ${OSGCAL_INCLUDE_DIR}/osgCal/*  )
FILE(GLOB cpp_files ${OSGCAL_SOURCE_DIR}/osgCal/*.cpp) #${OSGCAL_SOURCE_DIR}/osgCal/shaders/*.h )
#message(${cpp_files})

ADD_LIBRARY(${LIB_NAME} SHARED ${BUILD_MODE} ${LIB_PUBLIC_HEADERS} ${cpp_files})

LINK_INTERNAL(${LIB_NAME} osg osgUtil osgDB OpenThreads)
LINK_EXTERNAL(${LIB_NAME} cal3d ${OPENGL_LIBRARIES})

SET_TARGET_PROPERTIES(${LIB_NAME} PROPERTIES PROJECT_LABEL "core_${LIB_NAME}")

INCLUDE(ModuleInstall OPTIONAL)
