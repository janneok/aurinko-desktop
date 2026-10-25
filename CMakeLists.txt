cmake_minimum_required(VERSION 3.21)

project(AurinkoDesktop VERSION 0.8.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Quick QuickControls2)
qt_standard_project_setup(REQUIRES 6.5)

# CMake-projektiversio on ainoa versionumeron lähde.
# Main.cpp antaa sen Qt:n Application.version-ominaisuudelle.
configure_file(src/Version.h.in
    "${CMAKE_CURRENT_BINARY_DIR}/generated/Version.h" @ONLY)

qt_add_executable(aurinko-desktop
    src/main.cpp
    src/SolarCalculator.cpp
    src/SolarCalculator.h
)
target_include_directories(aurinko-desktop PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}/generated")

# Kuvake sisällytetään suoraan binääriin: näkyy myös kehitysajossa.
qt_add_resources(aurinko-desktop "aurinko-icon"
    PREFIX "/"
    FILES resources/aurinko.png
)

# JS-apukirjastoja tuodaan suoraan QML-tiedostoihin; niitä ei listata
# qmldir:n JavaScript-moduleina. Qt 6.11:n kehitysvaroitukset poistuvat.
set_source_files_properties(CalendarLogic.js SkyPalette.js PROPERTIES
    QT_QML_SKIP_QMLDIR_ENTRY TRUE
)

qt_add_qml_module(aurinko-desktop
    URI Aurinko
    VERSION 1.0
    QML_FILES
        Main.qml
        SidebarEntry.qml
        MetricCard.qml
        SunChart.qml
        TimeZonePicker.qml
        CalendarLogic.js
        SkyPalette.js
)

target_link_libraries(aurinko-desktop
    PRIVATE Qt6::Quick Qt6::QuickControls2
)

set_target_properties(aurinko-desktop PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

# Asenna käyttäjäkohtaisesti: cmake --install build --prefix "$HOME/.local"
install(TARGETS aurinko-desktop RUNTIME DESTINATION bin)
install(FILES resources/fi.oksanen.Aurinko.desktop DESTINATION share/applications)
install(FILES resources/fi.oksanen.Aurinko.svg DESTINATION share/icons/hicolor/scalable/apps)
install(FILES flatpak/fi.oksanen.Aurinko.metainfo.xml DESTINATION share/metainfo)

# Perustesti voidaan ajaa myös ilman graafista työpöytää: ctest --test-dir build --output-on-failure
include(CTest)
if(BUILD_TESTING)
    add_executable(aurinko-solar-smoke
        tests/solar_smoke.cpp
        src/SolarCalculator.cpp
        src/SolarCalculator.h
    )
    target_include_directories(aurinko-solar-smoke PRIVATE src)
    target_link_libraries(aurinko-solar-smoke PRIVATE Qt6::Core)
    add_test(NAME solar-smoke COMMAND aurinko-solar-smoke)
endif()

if(BUILD_TESTING)
    add_executable(aurinko-solar-json
        tests/solar_json.cpp
        src/SolarCalculator.cpp
        src/SolarCalculator.h
    )
    target_include_directories(aurinko-solar-json PRIVATE src)
    target_link_libraries(aurinko-solar-json PRIVATE Qt6::Core)
endif()

# Auditointisäännöstön testit ovat Pythonin vakio unittestillä
# eivätkä tarvitse Astralia tai verkkoa.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(BUILD_TESTING AND Python3_Interpreter_FOUND)
    add_test(NAME audit-comparison-unit
        COMMAND ${Python3_EXECUTABLE} -m unittest discover
            -s ${CMAKE_CURRENT_SOURCE_DIR}/tests
            -p test_compare_aurinko_api.py)
endif()

# Estää versionumeron vanhenemisen QML:ssä ja varmistaa hakuvalitsimen
# pysymisen mukana CMake-julkaisussa. Toimii myös ilman näyttöä tai verkkoa.
if(BUILD_TESTING AND Python3_Interpreter_FOUND)
    add_test(NAME release-metadata
        COMMAND ${Python3_EXECUTABLE} -m unittest discover
            -s ${CMAKE_CURRENT_SOURCE_DIR}/tests
            -p test_release_metadata.py)
endif()

# v0.8: kalenderin puhtaat JS-säännöt ja dynaamisen taivaan värit testataan
# Node.js:llä, jos se on käytettävissä. Qt-sovellus toimii ilman Nodea.
find_program(NODE_EXECUTABLE NAMES node nodejs)
if(BUILD_TESTING AND NODE_EXECUTABLE)
    add_test(NAME live-calendar-and-sky
        COMMAND ${NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_live_calendar.js)
endif()

# Flatpak-paketin ID:t, minimioikeudet ja metadata tarkistetaan ilman verkkoa.
if(BUILD_TESTING AND Python3_Interpreter_FOUND)
    add_test(NAME flatpak-packaging
        COMMAND ${Python3_EXECUTABLE} -m unittest discover
            -s ${CMAKE_CURRENT_SOURCE_DIR}/tests
            -p test_flatpak_packaging.py)
endif()
