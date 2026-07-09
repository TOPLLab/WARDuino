include_guard(GLOBAL)

find_package(doctest CONFIG QUIET)

if (NOT doctest_FOUND)
    include(FetchContent)

    FetchContent_Declare(
            doctest
            GIT_REPOSITORY https://github.com/doctest/doctest.git
            GIT_TAG v2.4.12
    )

    FetchContent_MakeAvailable(doctest)
endif ()

if (NOT TARGET doctest::doctest)
    message(FATAL_ERROR "Doctest target doctest::doctest was not found")
endif ()

