file(GLOB SERVER_SOURCES
    src/*.cpp
)

add_executable(server
    ${SERVER_SOURCES}
)

target_include_directories(server
    PRIVATE
        include
)

target_compile_options(server
    PRIVATE
        -Wall
        -Wextra
        -Werror
        -pedantic
)

target_compile_features(server PRIVATE cxx_std_17)
