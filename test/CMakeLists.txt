# Copyright (c)  Zubax Robotics  <zubax.com>
# Author: Pavel Kirienko <pavel.kirienko@zubax.com>

add_executable(tolstoy_test
    main.cpp
    text_test.cpp
    json_test.cpp
    tsv_test.cpp
)

target_link_libraries(tolstoy_test PRIVATE tolstoy)

target_include_directories(tolstoy_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Apply strict warnings to our own test TUs. doctest.h is included via the user header search path in main.cpp
# (not a dependency header), and only main.cpp includes it with DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN; we silence that
# TU separately below because doctest's single header is not warning-clean under our flag set.
set(tolstoy_test_strict_flags
    -Werror -Wall -Wextra
    -Wdouble-promotion -Wswitch-enum -Wfloat-equal -Wundef -Wconversion -Wtype-limits
    -Wsign-conversion -Wcast-align -Wmissing-declarations -Wzero-as-null-pointer-constant
    -Wnon-virtual-dtor -Woverloaded-virtual -Wsign-promo -Wold-style-cast -Wmissing-field-initializers
    # doctest.h emits a #warning about <ciso646> on libstdc++ >= 15; not something we can fix in the framework.
    -Wno-\#warnings
)

foreach(src text_test.cpp json_test.cpp tsv_test.cpp)
    set_source_files_properties(${src} PROPERTIES COMPILE_OPTIONS "${tolstoy_test_strict_flags}")
endforeach()

# doctest.h triggers many warnings under our strict flag set; the runner TU is isolated.
set_source_files_properties(main.cpp PROPERTIES COMPILE_OPTIONS "-w")

add_test(NAME tolstoy_test COMMAND tolstoy_test)
