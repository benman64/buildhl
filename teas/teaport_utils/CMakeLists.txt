file(GLOB src ./*.cpp)

add_library(teaport_utils ${src})
target_link_libraries(teaport_utils PUBLIC sqlite3)
target_include_directories(teaport_utils PUBLIC
    ./
    ${TEAPORT_SRC_DIR}
    ${TEAPORT_SRC_DIR}/nlohmann_json
)