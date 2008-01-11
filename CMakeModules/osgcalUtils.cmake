#
# OSGCAL_LINK_LIBRARIES
#
MACRO(OSGCAL_LINK_LIBRARIES TARGETNAME)
    FOREACH(varname ${ARGN})
            TARGET_LINK_LIBRARIES(${TARGETNAME} optimized "${varname}" debug "${varname}${OSGCAL_DEBUG_POSTFIX}")
    ENDFOREACH(varname)
ENDMACRO(OSGCAL_LINK_LIBRARIES TARGETNAME)


#
# OSGCAL_APPLICATION
#
# First argument (TARGET_NAME) is the name of the example to be build
# The source file application_<TARGET_NAME>.cpp will be used.
# Additional source files given as argument will be used to build the executable.
#
MACRO(OSGCAL_APPLICATION TARGET_NAME)

  SET(SOURCE_LIST ${ARGV})
  # Remove the first argument which is the target name
  LIST(REMOVE_ITEM SOURCE_LIST ${TARGET_NAME} )
  ADD_EXECUTABLE(${TARGET_NAME} ${SOURCE_LIST})

  SET_TARGET_PROPERTIES(${TARGET_NAME} PROPERTIES DEBUG_POSTFIX ${OSGCAL_DEBUG_POSTFIX})
  SET_TARGET_PROPERTIES(${TARGET_NAME} PROPERTIES PROJECT_LABEL "application_${TARGET_NAME}")

  INSTALL(TARGETS ${TARGET_NAME} RUNTIME DESTINATION bin ARCHIVE DESTINATION lib LIBRARY DESTINATION bin )
ENDMACRO(OSGCAL_APPLICATION TARGET_NAME SOURCE_FILES)



#
# OSGCAL_EXAMPLE
#
# First argument (TARGET_NAME) is the name of the example to be build
# The source file example_<TARGET_NAME>.cpp will be used.
# Additional source files given as argument will be used to build the executable.
#
MACRO(OSGCAL_EXAMPLE TARGET_NAME)
  
 SET(SOURCE_LIST ${ARGV} )
 
 LIST(REMOVE_ITEM SOURCE_LIST ${TARGET_NAME} )
   
  # There could be a headerfile with the same name, but it doesnt have to
  IF (EXISTS example_${TARGET_NAME}.h)
    SET(H_FILE example_${TARGET_NAME}.h)
  ENDIF (EXISTS example_${TARGET_NAME}.h)
  
  
  ADD_EXECUTABLE(${TARGET_NAME}  example_${TARGET_NAME}.cpp ${SOURCE_LIST} ${H_FILE} )

  SET_TARGET_PROPERTIES(${TARGET_NAME} PROPERTIES DEBUG_POSTFIX "d")
  SET_TARGET_PROPERTIES(${TARGET_NAME} PROPERTIES PROJECT_LABEL "example_${TARGET_NAME}")

  INSTALL(TARGETS ${TARGET_NAME} RUNTIME DESTINATION bin ARCHIVE DESTINATION lib LIBRARY DESTINATION bin )
  
ENDMACRO(OSGCAL_EXAMPLE TARGET_NAME)

