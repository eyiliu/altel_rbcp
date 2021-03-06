add_library(telgl SHARED src/TelescopeGL.cc src/TelescopeGL_shaders.cc)
target_link_libraries(telgl
  PRIVATE GL GLEW glut
  PUBLIC sfml-window  sfml-system
  )
set(LIB_PUBLIC_HEADERS include/TelescopeGL.hh  include/TelescopeViewer.hh)
set_target_properties(telgl PROPERTIES PUBLIC_HEADER "${LIB_PUBLIC_HEADERS}")
target_include_directories(telgl
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
  )

add_executable(mygl_reader TelescopeGL_main.cxx)
target_link_libraries(mygl_reader PRIVATE telgl mycommon telescope)

add_executable(mytel_gl telescope_main.cc)
target_link_libraries(mytel_gl PRIVATE telgl mycommon telescope)

add_executable(myfit myfit_main.cc)
target_link_libraries(myfit PRIVATE telgl mycommon telescope)


install(TARGETS telgl mytel_gl mygl_reader myfit
  EXPORT ${PROJECT_NAME}Targets
  RUNTIME       DESTINATION bin      COMPONENT runtime
  LIBRARY       DESTINATION lib      COMPONENT runtime
  ARCHIVE       DESTINATION lib      COMPONENT devel
  PUBLIC_HEADER DESTINATION include  COMPONENT devel
  RESOURCE      DESTINATION resource COMPONENT runtime
  )
