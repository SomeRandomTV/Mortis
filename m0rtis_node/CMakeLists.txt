file(GLOB VNODE_SOURCES
    src/*.cpp
)

add_executable(m0rtis_node
    ${VNODE_SOURCES}
)

target_include_directories(m0rtis_node 
    PRIVATE
        include
)

target_compile_options(m0rtis_node
    PRIVATE
        -Wall
        -Wextra
        -Werror
        -pedantic
)

target_compile_features(m0rtis_node PRIVATE cxx_std_20)
