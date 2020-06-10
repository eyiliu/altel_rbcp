
add_subdirectory(rbcp)

aux_source_directory(src LIB_SRC)
add_library(telescope SHARED ${LIB_SRC} )
target_link_libraries(telescope PRIVATE mycommon rbcp)
target_include_directories(telescope
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
  PRIVATE
  $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
  )


add_executable(mytest exe/FirmwarePortal_main.cc)
target_link_libraries(mytest PRIVATE mycommon telescope)

add_executable(mytele exe/telescope_main.cc)
target_link_libraries(mytele PRIVATE mycommon telescope)

add_executable(myrbcp exe/rbcp_main.cc)
target_link_libraries(myrbcp PRIVATE mycommon telescope)

install(TARGETS telescope mytest mytele myrbcp
  EXPORT ${PROJECT_NAME}Targets
  RUNTIME       DESTINATION bin      COMPONENT runtime
  LIBRARY       DESTINATION lib      COMPONENT runtime
  ARCHIVE       DESTINATION lib      COMPONENT devel
  PUBLIC_HEADER DESTINATION include  COMPONENT devel
  RESOURCE      DESTINATION resource COMPONENT runtime
  )
