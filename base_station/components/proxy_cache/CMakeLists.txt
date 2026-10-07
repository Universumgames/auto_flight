FILE(GLOB proxy_cache_srcs "*.cpp")

idf_component_register(
        SRCS ${proxy_cache_srcs}
        INCLUDE_DIRS "." "${CMAKE_CURRENT_BINARY_DIR}"
        REQUIRES flight_com nlohmann-json gps
)
