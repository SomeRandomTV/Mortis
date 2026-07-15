file(GLOB CLIENT_SOURCES
    src/*.cpp
)

add_executable(client
    ${CLIENT_SOURCES}
)

target_include_directories(client
    PRIVATE
        include
)

target_compile_options(client
    PRIVATE
        -Wall
        -Wextra
        -Werror
        -pedantic
)

target_compile_features(client PRIVATE cxx_std_17)
