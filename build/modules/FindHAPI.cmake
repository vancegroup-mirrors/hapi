# - Find HAPI
# Find the native HAPI headers and libraries.
#
#  HAPI_INCLUDE_DIR -  where to find HAPI.h, etc.
#  HAPI_LIBRARIES    - List of libraries when using HAPI.
#  HAPI_FOUND        - True if HAPI found.

GET_FILENAME_COMPONENT(module_file_path ${CMAKE_CURRENT_LIST_FILE} PATH )

# Look for the header file.
FIND_PATH(HAPI_INCLUDE_DIR NAMES HAPI/HAPI.h 
                           PATHS $ENV{H3D_ROOT}/../HAPI/include
                                 ../../HAPI/include
                                 ${module_file_path}/../../../HAPI/include
                           DOC "Path in which the file HAPI/HAPI.h is located." )
MARK_AS_ADVANCED(HAPI_INCLUDE_DIR)


# Look for the library.
IF( MSVC )
  SET( H3D_MSVC_VERSION 6 )
  SET( TEMP_MSVC_VERSION 1299 )
  WHILE( ${MSVC_VERSION} GREATER ${TEMP_MSVC_VERSION} )
    MATH( EXPR H3D_MSVC_VERSION "${H3D_MSVC_VERSION} + 1" )
    MATH( EXPR TEMP_MSVC_VERSION "${TEMP_MSVC_VERSION} + 100" )
  ENDWHILE( ${MSVC_VERSION} GREATER ${TEMP_MSVC_VERSION} )
  SET( HAPI_NAME "HAPI_vc${H3D_MSVC_VERSION}" )
ELSE(MSVC)
  SET( HAPI_NAME hapi )
ENDIF( MSVC )

FIND_LIBRARY(HAPI_LIBRARY NAMES ${HAPI_NAME}
                          PATHS $ENV{H3D_ROOT}/../lib
                                ../../lib
                                ${module_file_path}/../../../lib
                          DOC "Path to ${HAPI_NAME} library." )

FIND_LIBRARY( HAPI_DEBUG_LIBRARY NAMES ${HAPI_NAME}_d
              PATHS $ENV{H3D_ROOT}/../lib
                    ../../lib
                    ${module_file_path}/../../../lib
                    DOC "Path to ${HAPI_NAME}_d library." )
MARK_AS_ADVANCED(HAPI_LIBRARY)
MARK_AS_ADVANCED(HAPI_DEBUG_LIBRARY)

IF(HAPI_LIBRARY OR HAPI_DEBUG_LIBRARY)
  SET( HAVE_HAPI_LIBRARY 1 )
ELSE(HAPI_LIBRARY OR HAPI_DEBUG_LIBRARY)
  SET( HAVE_HAPI_LIBRARY 0 )
ENDIF(HAPI_LIBRARY OR HAPI_DEBUG_LIBRARY)

# Copy the results to the output variables.
IF(HAPI_INCLUDE_DIR AND HAVE_HAPI_LIBRARY)
  SET(HAPI_FOUND 1)
  IF(HAPI_LIBRARY)
    SET(HAPI_LIBRARIES ${HAPI_LIBRARIES} optimized ${HAPI_LIBRARY} )
  ELSE(HAPI_LIBRARY)
    SET(HAPI_LIBRARIES ${HAPI_LIBRARIES} optimized ${HAPI_NAME} )
    MESSAGE( STATUS "HAPI release libraries not found. Release build might not work." )
  ENDIF(HAPI_LIBRARY)

  IF(HAPI_DEBUG_LIBRARY)
    SET(HAPI_LIBRARIES ${HAPI_LIBRARIES} debug ${HAPI_DEBUG_LIBRARY} )
  ELSE(HAPI_DEBUG_LIBRARY)
    SET(HAPI_LIBRARIES ${HAPI_LIBRARIES} debug ${HAPI_NAME}_d )
    MESSAGE( STATUS "HAPI debug libraries not found. Debug build might not work." )
  ENDIF(HAPI_DEBUG_LIBRARY)
  
  SET(HAPI_INCLUDE_DIR ${HAPI_INCLUDE_DIR} )
  SET(HAPI_LIBRARIES ${HAPI_LIBRARIES} )
ELSE(HAPI_INCLUDE_DIR AND HAVE_HAPI_LIBRARY)
  SET(HAPI_FOUND 0)
  SET(HAPI_LIBRARIES)
  SET(HAPI_INCLUDE_DIR)
ENDIF(HAPI_INCLUDE_DIR AND HAVE_HAPI_LIBRARY)

# Report the results.
IF(NOT HAPI_FOUND)
  SET(HAPI_DIR_MESSAGE
    "HAPI was not found. Make sure HAPI_LIBRARY ( and/or HAPI_DEBUG_LIBRARY ) and HAPI_INCLUDE_DIR are set.")
  IF(HAPI_FIND_REQUIRED)
    MESSAGE(FATAL_ERROR "${HAPI_DIR_MESSAGE}")
  ELSEIF(NOT HAPI_FIND_QUIETLY)
    MESSAGE(STATUS "${HAPI_DIR_MESSAGE}")
  ENDIF(HAPI_FIND_REQUIRED)
ENDIF(NOT HAPI_FOUND)
