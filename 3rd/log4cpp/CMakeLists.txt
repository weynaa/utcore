# ----------------------------------------------------------------------------
#  CMake file for log4cpp. See root CMakeLists.txt
#
# ----------------------------------------------------------------------------

project(${LOG4CPP_LIBRARY} CXX)

include(CheckFunctionExists)
include(CheckIncludeFile)
include(CheckCSourceCompiles)
include(CheckTypeSize)

ut_include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_BINARY_DIR}")

# Version of LOG4CPP API.
SET( LOG4CPP_MAJOR_VERSION 0 )
SET( LOG4CPP_MINOR_VERSION 3 )
SET( LOG4CPP_BUILD_VERSION 5 )

SET(LOG4CPP_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# Version of project.
SET( LOG4CPP_FULL_VERSION ${LOG4CPP_MAJOR_VERSION}.${LOG4CPP_MINOR_VERSION}.${LOG4CPP_BUILD_VERSION} ) 

# All cpp files for LOG4CPP
FILE(GLOB LOG4CPP_SRCS RELATIVE "${LOG4CPP_SOURCE_DIR}"
   "${LOG4CPP_SOURCE_DIR}/src/*.cpp"
)

# All header files for LOG4CPP
FILE(GLOB LOG4CPP_HEADERS RELATIVE "${LOG4CPP_SOURCE_DIR}"
   "${LOG4CPP_SOURCE_DIR}/include/*.h"  "${LOG4CPP_SOURCE_DIR}/include/*/*.h" 
   "${LOG4CPP_SOURCE_DIR}/include/*.hh"  "${LOG4CPP_SOURCE_DIR}/include/*/*.hh" 
   "${LOG4CPP_SOURCE_DIR}/include/*/*/*.hh"
)

FIND_PACKAGE(PTHREAD REQUIRED)
IF(PTHREAD_FOUND)
  INCLUDE_DIRECTORIES( ${PTHREAD_INCLUDE_DIR} ) 
  SET(requiredLibs ${requiredLibs} ${PTHREAD_LIBRARIES} )
ENDIF(PTHREAD_FOUND)


# LOG4CPP shared library definition
ADD_LIBRARY(${LOG4CPP_LIBRARY} SHARED ${LOG4CPP_SRCS} ${LOG4CPP_HEADERS})
set_target_properties(${LOG4CPP_LIBRARY} PROPERTIES DEFINE_SYMBOL LOG4CPP_DLL)

IF(WIN32)
	target_link_libraries(${LOG4CPP_LIBRARY} wsock32)
ENDIF(WIN32)

# make the name of debug libraries end in _d.
SET_TARGET_PROPERTIES( ${LOG4CPP_LIBRARY} PROPERTIES DEBUG_POSTFIX "_d" )

# set the version of the library
SET_TARGET_PROPERTIES( ${LOG4CPP_LIBRARY} PROPERTIES VERSION ${LOG4CPP_FULL_VERSION} )

if(UNIX)
  if(CMAKE_COMPILER_IS_GNUCXX OR CV_ICC)
     set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
  endif()
endif()
if(WIN32)
  add_definitions("-DLOG4CPP_HAS_DLL")
  add_definitions("-DLOG4CPP_BUILD_DLL")
endif(WIN32)

set_target_properties(${LOG4CPP_LIBRARY} PROPERTIES
        OUTPUT_NAME ${LOG4CPP_LIBRARY}
        DEBUG_POSTFIX "${UBITRACK_DEBUG_POSTFIX}"
        ARCHIVE_OUTPUT_DIRECTORY ${3P_LIBRARY_OUTPUT_PATH}
    )

install(TARGETS ${LOG4CPP_LIBRARY}
  RUNTIME DESTINATION bin COMPONENT main
  LIBRARY DESTINATION ${UBITRACK_LIB_INSTALL_PATH} COMPONENT main
  ARCHIVE DESTINATION ${UBITRACK_LIB_INSTALL_PATH} COMPONENT main
  )

foreach(hdr ${LOG4CPP_HEADERS})
   string(REGEX REPLACE "^.*include" "" hdr2 "${hdr}")
   GET_FILENAME_COMPONENT(fpath ${hdr2} PATH)
   #MESSAGE(STATUS "${UBITRACK_INCLUDE_INSTALL_PATH}/${fpath}")
   IF(fpath)
     install(FILES ${hdr} DESTINATION "${UBITRACK_INCLUDE_INSTALL_PATH}/${fpath}" COMPONENT main)
   ELSE(fpath)
     install(FILES ${hdr} DESTINATION "${UBITRACK_INCLUDE_INSTALL_PATH}" COMPONENT main)
   ENDIF(fpath)
endforeach()

