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

add_executable(alarma
    main.c
    model/alarma.c
    view/ui.c
    controller/controller.c
)

target_link_libraries(alarma ${GTK3_LIBRARIES} ${LIBAO_LIBRARIES} ${LIBSNDFILE_LIBRARIES})

# Copiar alarma.wav al directorio de build tras compilar
add_custom_command(TARGET alarma POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/resource/alarma.wav $<TARGET_FILE_DIR:alarma>/alarma.wav)

# Copiar icono.png al directorio de build tras compilar
add_custom_command(TARGET alarma POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/resource/icono.png $<TARGET_FILE_DIR:alarma>/icono.png)

# Soporte para make install
install(TARGETS alarma RUNTIME DESTINATION bin)
install(FILES ${CMAKE_SOURCE_DIR}/resource/icono.png DESTINATION share/pixmaps)
install(FILES ${CMAKE_SOURCE_DIR}/alarma.desktop DESTINATION share/applications)
install(FILES ${CMAKE_SOURCE_DIR}/resource/alarma.wav DESTINATION share/sounds)
