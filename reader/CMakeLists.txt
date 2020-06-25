
add_executable(myviewer TelescopeViewer_main.cxx)
target_link_libraries(myviewer PRIVATE telgl mycommon telescope)

add_executable(myviewer_linefit TelescopeViewer_linefit.cxx)
target_link_libraries(myviewer_linefit PRIVATE telgl mycommon telescope)

install(TARGETS myviewer myviewer_linefit
  EXPORT ${PROJECT_NAME}Targets
  RUNTIME       DESTINATION bin      COMPONENT runtime
  LIBRARY       DESTINATION lib      COMPONENT runtime
  ARCHIVE       DESTINATION lib      COMPONENT devel
  PUBLIC_HEADER DESTINATION include  COMPONENT devel
  RESOURCE      DESTINATION resource COMPONENT runtime
  )
