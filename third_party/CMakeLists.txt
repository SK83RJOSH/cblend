if(CBLEND_BUILD_TESTS)
    # Catch2
    add_subdirectory(Catch2)
    target_disable_static_analysis(Catch2)
    target_disable_static_analysis(Catch2WithMain)
endif()

# expected
set(EXPECTED_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(EXPECTED_BUILD_PACKAGE OFF CACHE BOOL "" FORCE)
add_subdirectory(expected)
target_disable_static_analysis(expected)

# optional
set(OPTIONAL_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(OPTIONAL_BUILD_PACKAGE OFF CACHE BOOL "" FORCE)
add_subdirectory(optional)
target_disable_static_analysis(optional)

# optional
add_subdirectory(range-v3)
target_disable_static_analysis(range-v3-meta)
target_disable_static_analysis(range-v3-concepts)
target_disable_static_analysis(range-v3)

