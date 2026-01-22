# 🍒 ConsoleC: 핵심 기능 가이드 (Cherry-Picking)

**ConsoleC**는 거대한 프레임워크가 아닙니다.  
필요한 기능만 쏙쏙 뽑아서(Cherry-pick) 여러분의 C 프로그램에 강력한 기능을 더할 수 있습니다.

복잡한 `malloc`, `free`, `buffer` 없이도 바로 사용할 수 있는 3가지 핵심 기능을 소개합니다.

---

## 1. "한글/이모지 문자열 길이, 제대로 세고 싶어요!"

C언어의 `strlen`은 바이트 수만 알려줍니다. 한글("안녕")은 6바이트지만, 화면에서는 4칸을 차지하죠. UI가 깨지는 주범입니다.  
`cc_util.h`만 있으면 이 문제를 한 줄로 해결할 수 있습니다.

### 📌 `cc_util_get_string_width`

```c
#include "console_c.h"
#include <stdio.h>
#include <string.h>

int main() {
    const char* text1 = "Hello";
    const char* text2 = "안녕하세요"; // 한글 5글자 -> 화면 10칸 차지
    const char* text3 = "✨Star";     // 이모지 포함

    printf("--- [기존 strlen vs ConsoleC] ---\n");
    
    // 기존 방식 (실패)
    printf("'%s' length: %lu (화면 칸수랑 다름!)\n", text2, strlen(text2)); 

    // ConsoleC 방식 (성공)
    size_t width = cc_util_get_string_width(text2);
    printf("'%s' width : %lu (화면에서 차지하는 칸수)\n", text2, width);

    // 응용: 우측 정렬 출력하기 (화면 너비 20칸)
    int padding = 20 - (int)cc_util_get_string_width(text3);
    for(int i=0; i<padding; ++i) printf(" ");
    printf("%s\n", text3);

    return 0;
}

```

---

## 2. "scanf 말고, 게임처럼 키보드 입력을 받고 싶어요!"

`scanf`는 엔터를 칠 때까지 프로그램이 멈춥니다. 게임이나 도구를 만들려면 키를 누르는 즉시 반응해야 합니다.  
`cc_device` 모듈은 복잡한 설정 없이 `init` 한 줄이면 비동기 입력을 처리해줍니다.

### 📌 `cc_device_get_input`

```c
#include "console_c.h"
#include <stdio.h>

int main() {
    // 1. 마법의 주문 (터미널을 게임 모드로 전환)
    cc_device_init();

    printf("아무 키나 눌러보세요. (ESC: 종료)\n");

    int running = 1;
    while(running) {
        // 2. 키 입력 확인 (10ms 동안 기다림, 없으면 통과)
        cc_key_code_e key = cc_device_get_input(10);

        if (key == CC_KEY_NONE) {
            // 키 입력이 없을 때도 게임 로직은 계속 돌아갑니다!
            // printf("."); 
            continue;
        }

        // 3. 키 처리
        if (key == CC_KEY_ESC) {
            running = 0;
        } else {
            printf("\r입력된 키 코드: %d   ", key);
            fflush(stdout);
        }
    }

    // 4. 정리 (반드시 호출해야 터미널이 고장나지 않습니다)
    cc_device_deinit();
    return 0;
}

```

---

## 3. "터미널에서 마우스를 쓰고 싶어요!"

C언어 콘솔창에서 마우스를 쓰는 건 복잡한 기술(ANSI 파싱)이 필요하지만, ConsoleC를 쓰면 `inspect` 함수 하나로 끝납니다.

### 📌 `cc_device_inspect` (마우스 파싱)

```c
#include "console_c.h"
#include <stdio.h>

int main() {
    cc_device_init();

    // 1. 마우스 기능 켜기
    cc_device_enable_mouse(true);

    printf("화면을 클릭하거나 드래그해보세요. (Q: 종료)\n");

    while(1) {
        cc_key_code_e key = cc_device_get_input(10);

        if (key == CC_KEY_q) break;

        // 2. 마우스 이벤트인지 확인
        if (key == CC_KEY_MOUSE_EVENT) {
            // 3. 이벤트 상세 정보 열어보기 (Parsing)
            cc_input_event_t evt;
            cc_device_inspect(key, &evt);

            int x = evt._data._mouse._x; // 0-based 좌표
            int y = evt._data._mouse._y;
            
            // 클릭한 위치로 커서를 옮겨서 메시지 출력
            cc_screen_move_cursor(x, y);
            
            if (evt._data._mouse._action == CC_MOUSE_ACTION_PRESS) {
                printf("Click!");
            } else if (evt._data._mouse._action == CC_MOUSE_ACTION_DRAG) {
                printf("Drag~");
            }
            fflush(stdout);
        }
    }

    cc_device_enable_mouse(false);
    cc_device_deinit();
    return 0;
}

```

---

## 4. [종합 예제] 50줄로 만드는 '캐릭터 움직이기'

위의 기능들을 합쳐서, 방향키로 `@` 캐릭터를 움직이는 초간단 게임 예제입니다. 더블 버퍼링 없이 화면 제어 함수(`move_cursor`)만 사용합니다.

```c
#include "console_c.h"
#include <stdio.h>

int main() {
    cc_device_init();
    cc_screen_clear(); // 화면 지우기

    int x = 10, y = 5; // 캐릭터 시작 위치
    
    // 초기 캐릭터 그리기
    cc_screen_move_cursor(x, y);
    printf("@");
    fflush(stdout);

    while(1) {
        cc_key_code_e key = cc_device_get_input(10);
        if (key == CC_KEY_NONE) continue;
        if (key == CC_KEY_q) break;

        // 1. 기존 위치 지우기 (공백 덮어쓰기)
        cc_screen_move_cursor(x, y);
        printf(" ");

        // 2. 좌표 이동
        if (key == CC_KEY_UP)    y--;
        if (key == CC_KEY_DOWN)  y++;
        if (key == CC_KEY_LEFT)  x--;
        if (key == CC_KEY_RIGHT) x++;

        // 3. 새 위치에 그리기
        cc_screen_move_cursor(x, y);
        printf("@");
        
        // 4. 커서를 엉뚱한 곳으로 치워두기 (깜빡임 방지 팁)
        cc_screen_move_cursor(0, 0); 
        fflush(stdout);
    }

    cc_device_deinit();
    return 0;
}

```

---

## 💡 요약: 이것만 기억하세요!

1. **초기화:** `main` 시작 부분에 `cc_device_init()`, 끝 부분에 `cc_device_deinit()`만 넣으세요.
2. **입력:** `cc_device_get_input(ms)`로 키보드를 기다리지 않고 받으세요.
3. **마우스:** `cc_device_inspect()`를 쓰면 복잡한 마우스 좌표 계산을 알아서 해줍니다.
4. **한글:** 문자열 길이가 이상하다 싶으면 `cc_util_get_string_width()`를 쓰세요.

이것만 알면 여러분도 C언어로 멋진 인터랙티브 프로그램을 만들 수 있습니다!