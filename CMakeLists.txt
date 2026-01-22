cmake_minimum_required(VERSION 3.10)
project(ConsoleC LANGUAGES C)

# C Standard 설정 (C11)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 컴파일 옵션 (Warning 등)
add_compile_options(-Wall -Wextra)

# include 디렉토리 설정
include_directories(include)

# 소스 파일 목록
set(SOURCES
    src/cc_color.c
    src/cc_util.c
    src/cc_buffer.c
    src/cc_screen.c
    src/cc_device.c
)

# 라이브러리 생성 (Static Library)
add_library(console_c STATIC ${SOURCES})

# Thread 라이브러리 링크 (cc_device에서 pthread 사용)
find_package(Threads REQUIRED)
target_link_libraries(console_c PRIVATE Threads::Threads)

# 버퍼 단위 콘솔 출력 예제
add_executable(buffer_test example/main_buffer_test.c)
target_link_libraries(buffer_test PRIVATE console_c)

# 입력 테스터기 예제
add_executable(input_test example/main_input_test.c)
target_link_libraries(input_test PRIVATE console_c)

# 그림판
add_executable(draw_app example/main_draw_app.c)
target_link_libraries(draw_app PRIVATE console_c)

# 인벤토리
add_executable(inventory_app example/main_inventory_app.c)
target_link_libraries(inventory_app PRIVATE console_c)