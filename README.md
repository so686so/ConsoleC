# ConsoleC

<div align="center">

**High-Performance TUI Library for C**

**[ConsoleX](https://github.com/so686so/ConsoleX)의 C 언어 포팅 프로젝트**

</div>

---

## 📖 개요 (Overview)

**ConsoleC**는 모던 C++ TUI 라이브러리인 **ConsoleX**를 **C11** 표준에 맞춰 포팅한 프로젝트입니다.
기존 ConsoleX가 제공하던 강력한 더블 버퍼링, TrueColor 지원, 스레드 안전한 입력 처리 기능을 순수 C 언어 환경에서도 사용할 수 있도록 재설계했습니다.

임베디드 리눅스나 시스템 프로그래밍 환경 등 C++ 런타임이 무겁거나 사용할 수 없는 환경에서 고성능 콘솔 애플리케이션을 제작하는 데 최적화되어 있습니다.

---

## 📚 문서 및 가이드 (Documentation & Guides)

ConsoleC는 초급자부터 고급 사용자까지 모두를 위한 가이드를 제공합니다. 자신의 목적에 맞는 문서를 선택하세요.

### 🐣 **초급 개발자 / 부분 기능 필요 (Cherry-Picking)**

복잡한 구조 없이 **"키 입력 받기", "마우스 좌표 얻기", "문자열 길이 계산"** 등 필요한 기능만 쏙쏙 뽑아 쓰고 싶으신가요?

* 👉 **[BASIC_GUIDE.md](./BASIC_GUIDE.md) 보러 가기**
* `cc_device_get_input`: 비동기 키보드 입력
* `cc_device_inspect`: 마우스 이벤트 파싱
* `cc_util_get_string_width`: 한글/이모지 문자열 너비 계산



### 🏗️ **본격적인 TUI 애플리케이션 개발**

더블 버퍼링, 화면 깜빡임 방지, 객체 기반의 윈도우 관리 등 **완성도 높은 TUI 프로그램**을 만들고 싶으신가요?

* 👉 **[HOW_TO_MAKE_UI_USE_ConsoleC.md](./HOW_TO_MAKE_UI_USE_ConsoleC.md) 보러 가기**
* 단계별 UI 구현 가이드 (Step-by-Step)
* 상태(State) 기반 애플리케이션 구조 설계



---

## 💡 예제 갤러리 (Examples)

각 예제 코드는 `example/` 디렉토리에 있으며, ConsoleC의 핵심 기능을 보여줍니다.

| 파일명 | 설명 | 추천 대상 |
| --- | --- | --- |
| **[`main_buffer_test.c`](./example/main_buffer_test.c)** | **더블 버퍼링 성능 테스트.** 화면 갱신 속도와 색상 출력 기능을 테스트합니다. | **입문자** |
| **[`main_input_test.c`](./example/main_input_test.c)** | **입력 시스템 테스트.** 키보드 반응 속도, 마우스 클릭/드래그 좌표 로깅, 한글 입력 처리 등을 확인합니다. | **중급자** |
| **[`main_draw_app.c`](./example/main_draw_app.c)** | **마우스 그림판.** 터미널에서 마우스를 이용해 그림을 그리고 색상을 변경하는 인터랙티브 앱입니다. | **중급자** |
| **[`main_inventory_app.c`](./example/main_inventory_app.c)** | **종합 UI 데모.** 윈도우 드래그 & 드롭, 아이템 이동, 레이아웃 관리 등 복잡한 UI 로직을 포함합니다. | **고급자** |

---

## ✨ 주요 기능 (Features)

* **고성능 더블 버퍼링 (High-Performance Double Buffering)**
* 화면의 변경된 부분(Diff)만 계산하여 렌더링하므로 깜빡임이 없고 CPU 사용량이 낮습니다.
* `cc_buffer_t`: 1D 플랫 배열을 사용하여 캐시 효율성을 극대화했습니다.


* **TrueColor (RGB) 지원**
* 기본 ANSI 색상뿐만 아니라 1600만 가지의 TrueColor(RGB)를 지원합니다.
* Hex 코드 파싱 및 자동 색상 보정 기능을 제공합니다.


* **강력한 입력 처리 (Robust Input Handling)**
* `kbhit()`이나 `ncurses` 없이도 비동기 키보드/마우스 입력을 처리합니다.
* **POSIX 스레드** 기반의 이벤트 루프를 통해 입력 대기 중에도 애플리케이션이 멈추지 않습니다.
* 마우스 클릭, 드래그, 휠 이벤트 및 터미널 리사이즈(SIGWINCH)를 지원합니다.


* **UTF-8 멀티바이트 문자 지원**
* 한글(2칸)과 영문(1칸)의 너비를 정확히 계산하여 UI 깨짐을 방지합니다.


* **종속성 최소화**
* 외부 라이브러리 없이 표준 C 라이브러리와 POSIX 시스템 콜(`pthread`, `termios`, `ioctl`)만으로 동작합니다.



---

## 🛠 빌드 및 설치 (Build & Install)

**요구 사항 (Requirements)**

* C11 컴파일러 (gcc 7.5+ 권장)
* CMake 3.10 이상
* Linux / macOS (Windows 미지원 - POSIX 의존)
* Pthread 라이브러리

**빌드 방법**

```bash
# 1. 저장소 클론
git clone https://github.com/YourUsername/ConsoleC.git
cd ConsoleC

# 2. 빌드 디렉토리 생성
mkdir build && cd build

# 3. CMake 설정 및 빌드
cmake ..
make

# 4. 예제 실행
./input_test      # 입력 시스템 테스트 (추천)
./draw_app        # 마우스 그림판 예제
./inventory_app   # 윈도우 드래그 & 드롭 예제

```

---

## 🚀 사용법 (Usage: Quick Start)

ConsoleC는 `console_c.h` 헤더 하나로 모든 기능을 제공합니다.

### Hello World 예제

```c
#include "console_c.h"
#include <unistd.h> // for usleep

int main(void) {
    // 1. 시스템 초기화 (Raw Mode 진입, 이벤트 스레드 시작)
    cc_device_init();
    
    // 2. 버퍼 생성 (너비 80, 높이 24)
    cc_buffer_t* buffer = cc_buffer_create(80, 24);

    // 3. 그리기
    cc_buffer_clear(buffer, &CC_COLOR_BLACK); // 배경 검정
    
    // 문자열 출력 (좌표 10, 5)
    cc_buffer_draw_string(buffer, 10, 5, "Hello, ConsoleC!", &CC_COLOR_CYAN, &CC_COLOR_BLACK);
    
    // 박스 그리기
    cc_buffer_draw_box(buffer, 8, 4, 20, 3, &CC_COLOR_WHITE, &CC_COLOR_BLACK, false);

    // 4. 렌더링 (변경된 내용만 출력)
    cc_buffer_flush(buffer);

    // 잠시 대기
    usleep(2000000); 

    // 5. 정리 (메모리 해제 및 터미널 복구)
    cc_buffer_destroy(buffer);
    cc_device_deinit();
    
    return 0;
}

```

---

## 📂 디렉토리 구조 (Structure)

```text
ConsoleC/
├── BASIC_GUIDE.md                 # 초급자용 기능 가이드 (Cherry-picking)
├── HOW_TO_MAKE_UI_USE_ConsoleC.md # UI 개발 단계별 가이드
├── include/
│   ├── console_c.h                # 통합 헤더
│   └── console_c/                 # 모듈별 헤더 파일
│       ├── cc_buffer.h            # 화면 버퍼링 및 렌더링
│       ├── cc_color.h             # RGB 색상 처리
│       ├── cc_device.h            # 키보드/마우스 입력 제어
│       ├── cc_screen.h            # 터미널 커서 및 크기 제어
│       └── cc_util.h              # UTF-8 문자열 처리 유틸리티
├── src/                           # 소스 코드 (.c)
├── example/                       # 예제 코드
│   ├── main_buffer_test.c
│   ├── main_draw_app.c
│   ├── main_input_test.c
│   └── main_inventory_app.c
└── CMakeLists.txt

```

---

## 📦 프로젝트 통합 (Integration Guide)

라이브러리를 별도로 빌드하지 않고, **ConsoleC의 소스 코드를 귀하의 프로젝트에 직접 복사하여 사용**하려면 아래 절차를 따르십시오.

### 1. 파일 복사 (File Copy)

`include`와 `src` 디렉토리의 파일들을 귀하의 프로젝트 구조에 맞게 복사합니다.  
**주의 :** 헤더 파일의 경로 의존성을 위해 `include/console_c` 디렉토리 구조를 유지하는 것을 권장합니다.

**권장 디렉토리 구조:**

```text
YourProject/
├── include/
│   ├── console_c.h        <-- 통합 헤더 복사
│   └── console_c/         <-- include/console_c 폴더 전체 복사
├── src/
│   ├── cc_buffer.c        <-- src 폴더 내 모든 .c 파일 복사
│   ├── cc_color.c
│   ├── cc_device.c
│   ├── cc_screen.c
│   ├── cc_util.c
│   └── main.c             <-- 사용자의 메인 코드

```

### 2. 컴파일 옵션 (Compile Flags)

ConsoleC는 **POSIX Thread**를 사용하므로 컴파일 시 반드시 pthread 라이브러리를 링크해야 합니다.

**GCC / Clang 사용 시:**
헤더 경로(`-I`)를 지정하고 `-lpthread` 옵션을 추가합니다.

```bash
gcc -std=c11 -I./include \
    src/*.c \
    your_main.c \
    -lpthread -o your_app

```

**CMake 사용 시:**
기존 `CMakeLists.txt`에 ConsoleC의 소스 파일들을 추가하고 `Threads` 패키지를 링크합니다.

```cmake
# 1. Thread 라이브러리 검색
find_package(Threads REQUIRED)

# 2. ConsoleC 소스 파일 목록 정의
set(CONSOLEC_SOURCES
    src/cc_buffer.c
    src/cc_color.c
    src/cc_device.c
    src/cc_screen.c
    src/cc_util.c
)

# 3. 실행 파일 또는 라이브러리에 소스 및 링크 추가
add_executable(your_app main.c ${CONSOLEC_SOURCES})
target_include_directories(your_app PRIVATE include)
target_link_libraries(your_app PRIVATE Threads::Threads)

```

---

## ⚖️ ConsoleX와의 차이점 (vs ConsoleX)

| Feature | ConsoleX (C++) | ConsoleC (C) |
| --- | --- | --- |
| **패러다임** | 객체 지향 (OOP) | 절차 지향 / 데이터 중심 |
| **메모리 관리** | RAII (자동 해제) | 수동 관리 (`_create`, `_destroy`) |
| **문자열** | `std::string` | `char*`, `char[]` |
| **컨테이너** | `std::vector` | 동적 배열 + `realloc` |
| **스레딩** | `std::thread`, `std::future` | `pthread`, `atomic`, `select` |
| **네이밍** | `cx::Buffer::Draw()` | `cc_buffer_draw()` |

## 🔗 원본 프로젝트 (Original Project)

이 프로젝트는 **[ConsoleX](https://github.com/so686so/ConsoleX)** 의 구조와 로직을 기반으로 합니다.
C++ 환경에서 개발하신다면 원본 프로젝트를 사용하시기를 권장합니다.

## 📄 라이선스 (License)

이 프로젝트는 MIT 라이선스를 따릅니다. 자세한 내용은 `LICENSE` 파일을 참고하세요.