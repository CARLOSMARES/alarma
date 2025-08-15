cmake_minimum_required(VERSION 3.10)
project(alarma C)

set(CMAKE_C_STANDARD 99)


find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
pkg_check_modules(LIBAO REQUIRED ao)
pkg_check_modules(LIBSNDFILE REQUIRED sndfile)


include_directories(
    ${GTK3_INCLUDE_DIRS}
    ${LIBAO_INCLUDE_DIRS}
    ${LIBSNDFILE_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/model
    ${CMAKE_SOURCE_DIR}/view
    ${CMAKE_SOURCE_DIR}/controller
    ${CMAKE_SOURCE_DIR}
)


link_directories(${GTK3_LIBRARY_DIRS} ${LIBAO_LIBRARY_DIRS} ${LIBSNDFILE_LIBRARY_DIRS})
add_definitions(${GTK3_CFLAGS_OTHER} ${LIBAO_CFLAGS_OTHER} ${LIBSNDFILE_CFLAGS_OTHER})


# --- GResource: empaquetar icono.png y alarma.wav ---
find_program(GLIB_COMPILE_RESOURCES glib-compile-resources)
set(GRESOURCE_XML ${CMAKE_SOURCE_DIR}/resource/alarma.gresource.xml)
set(GRESOURCE_C ${CMAKE_BINARY_DIR}/alarma-resources.c)

add_custom_command(
    OUTPUT ${GRESOURCE_C}
    COMMAND ${GLIB_COMPILE_RESOURCES}
        --target=${GRESOURCE_C}
        --generate-source
        --sourcedir=${CMAKE_SOURCE_DIR}/resource
        ${GRESOURCE_XML}
    DEPENDS ${GRESOURCE_XML} ${CMAKE_SOURCE_DIR}/resource/icono.png ${CMAKE_SOURCE_DIR}/resource/alarma.wav
)

set(GRESOURCE_SRC ${GRESOURCE_C})

add_executable(alarma
    main.c
    model/alarma.c
    view/ui.c
    controller/controller.c
    ${GRESOURCE_SRC}
)

target_link_libraries(alarma ${GTK3_LIBRARIES} ${LIBAO_LIBRARIES} ${LIBSNDFILE_LIBRARIES})
install(FILES ${CMAKE_SOURCE_DIR}/resource/icono.png DESTINATION share/pixmaps)
install(FILES ${CMAKE_SOURCE_DIR}/alarma.desktop DESTINATION share/applications)
install(FILES ${CMAKE_SOURCE_DIR}/resource/alarma.wav DESTINATION share/sounds)
