IF(WIN32)
  cmake_minimum_required(VERSION 2.6.0)
ENDIF(WIN32)
# This will set up three projects. These three projects are examples
# from the manual.
project (ThreadExamples)

# set the install directory on Windows
IF( WIN32 AND CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT AND NOT H3D_CMAKE_INSTALL_PREFIX_ALREADY_SET )
  SET( CMAKE_INSTALL_PREFIX ${ThreadExamples_SOURCE_DIR}/../.. CACHE PATH "Install path prefix, prepended onto install directories." FORCE )
  SET( H3D_CMAKE_INSTALL_PREFIX_ALREADY_SET TRUE )
ENDIF( WIN32 AND CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT AND NOT H3D_CMAKE_INSTALL_PREFIX_ALREADY_SET )

# use, i.e. don't skip the full RPATH for the build tree
SET(CMAKE_SKIP_BUILD_RPATH FALSE)

# when building, don't use the install RPATH already
# (but later on when installing)
SET(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE) 

# add the automatically determined parts of the RPATH
# which point to directories outside the build tree to the install RPATH
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# the RPATH to be used when installing, but only if it's not a system directory
LIST(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/lib" isSystemDir)
IF("${isSystemDir}" STREQUAL "-1")
   SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
ENDIF("${isSystemDir}" STREQUAL "-1")

SET( SimpleThreadPrint_SRCS "${ThreadExamples_SOURCE_DIR}/../SimpleThreadPrint/SimpleThreadPrint.cpp" )
SET( SimpleThreadPrintLock_SRCS "${ThreadExamples_SOURCE_DIR}/../SimpleThreadPrintLock/SimpleThreadPrintLock.cpp" )
SET( PeriodicThreadCallbacks_SRCS "${ThreadExamples_SOURCE_DIR}/../PeriodicThreadCallbacks/PeriodicThreadCallbacks.cpp" )

SET(requiredLibs)

SET(CMAKE_MODULE_PATH ${ThreadExamples_SOURCE_DIR}/../../../build/modules )

IF( TARGET H3DUtil )
  INCLUDE_DIRECTORIES( ${H3DUTIL_INCLUDE_DIR} ) 
  SET( requiredLibs ${requiredLibs} H3DUtil )
ELSE( TARGET H3DUtil )
  #H3DUtil
  FIND_PACKAGE(H3DUtil REQUIRED)

  IF(H3DUTIL_FOUND)
    INCLUDE_DIRECTORIES( ${H3DUTIL_INCLUDE_DIR} ) 
    SET(requiredLibs ${requiredLibs} ${H3DUTIL_LIBRARIES} )
  ENDIF(H3DUTIL_FOUND)
ENDIF( TARGET H3DUtil )

IF( H3D_USE_DEPENDENCIES_ONLY )
  # The variables set here must be set by the CMakeLists.txt that sets H3D_USE_DEPENDENCIES_ONLY to true.
  IF(PTHREAD_FOUND)
    INCLUDE_DIRECTORIES( ${PTHREAD_INCLUDE_DIR} ) 
  ENDIF(PTHREAD_FOUND)
  INCLUDE_DIRECTORIES( ${EXTERNAL_INCLUDE_DIR} ) 
ELSE( H3D_USE_DEPENDENCIES_ONLY )
  FIND_PACKAGE(PTHREAD REQUIRED)
  IF(PTHREAD_FOUND)
    INCLUDE_DIRECTORIES( ${PTHREAD_INCLUDE_DIR} ) 
    SET(requiredLibs ${requiredLibs} ${PTHREAD_LIBRARIES} )
  ENDIF(PTHREAD_FOUND)
ENDIF( H3D_USE_DEPENDENCIES_ONLY )

ADD_EXECUTABLE(SimpleThreadPrint ${SimpleThreadPrint_SRCS})
ADD_EXECUTABLE(SimpleThreadPrintLock ${SimpleThreadPrintLock_SRCS})
ADD_EXECUTABLE(PeriodicThreadCallbacks ${PeriodicThreadCallbacks_SRCS})

# make the name of debug libraries end in _d.
SET_TARGET_PROPERTIES( SimpleThreadPrint PROPERTIES DEBUG_POSTFIX "_d" )
SET_TARGET_PROPERTIES( SimpleThreadPrintLock PROPERTIES DEBUG_POSTFIX "_d" )
SET_TARGET_PROPERTIES( PeriodicThreadCallbacks PROPERTIES DEBUG_POSTFIX "_d" )

TARGET_LINK_LIBRARIES( SimpleThreadPrint ${requiredLibs})
TARGET_LINK_LIBRARIES( SimpleThreadPrintLock ${requiredLibs})
TARGET_LINK_LIBRARIES( PeriodicThreadCallbacks ${requiredLibs})

INSTALL( TARGETS SimpleThreadPrint
	 LIBRARY DESTINATION lib COMPONENT HAPI_cpack_examples_runtime
	 RUNTIME DESTINATION bin COMPONENT HAPI_cpack_examples_runtime )

INSTALL( TARGETS SimpleThreadPrintLock
	 LIBRARY DESTINATION lib COMPONENT HAPI_cpack_examples_runtime
	 RUNTIME DESTINATION bin COMPONENT HAPI_cpack_examples_runtime )

INSTALL( TARGETS PeriodicThreadCallbacks
	 LIBRARY DESTINATION lib COMPONENT HAPI_cpack_examples_runtime
	 RUNTIME DESTINATION bin COMPONENT HAPI_cpack_examples_runtime )
