file(GLOB HUB_SOURCES
    src/*.cpp
)

add_executable(m0rtis_hub
    ${HUB_SOURCES}
)

target_include_directories(m0rtis_hub
    PRIVATE
        include
)
target_link_libraries(m0rtis_hub 
    PRIVATE 
        m0rtis_proto
)
target_compile_options(m0rtis_hub
    PRIVATE
        -Wall
        -Wextra
        -Werror
        -pedantic
)

target_compile_features(m0rtis_hub PRIVATE cxx_std_20)
