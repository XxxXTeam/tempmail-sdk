cmake_minimum_required(VERSION 3.14)
project(tempmail_sdk C)

set(CMAKE_C_STANDARD 11)

# 查找 libcurl
find_package(CURL REQUIRED)

# SDK 库
add_library(tempmail_sdk STATIC
    src/types.c
    src/logger.c
    src/retry.c
    src/normalize.c
    src/client.c
    src/http.c
    src/providers/tempmail.c
    src/providers/linshi_email.c
    src/providers/tempmail_lol.c
    src/providers/chatgpt_org_uk.c
    src/providers/tempmail_la.c
    src/providers/temp_mail_io.c
    src/providers/awamail.c
    src/providers/mail_tm.c
    src/providers/dropmail.c
    src/providers/guerrillamail.c
    src/providers/maildrop.c
    vendor/cJSON.c
)

target_include_directories(tempmail_sdk PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor
)

target_link_libraries(tempmail_sdk PUBLIC CURL::libcurl)

# 示例程序
add_executable(tempmail_example example/main.c)
target_link_libraries(tempmail_example tempmail_sdk)
