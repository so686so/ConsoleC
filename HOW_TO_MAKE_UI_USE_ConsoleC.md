# 📚 ConsoleC UI 구현 가이드

## 📋 목차

1. **Step 1**: 기본 골격 (Boilerplate)
2. **Step 2**: 더블 버퍼링 & 렌더링 루프
3. **Step 3**: 입력 처리 (키보드)
4. **Step 4**: 마우스 상호작용 추가
5. **Step 5**: 앱 구조화 (State Pattern)

---

## Step 1: 기본 골격 (Boilerplate)

모든 ConsoleC 애플리케이션의 시작점입니다. 시스템을 초기화(`init`)하고, 프로그램이 종료될 때 설정을 복구(`deinit`)하는 것이 가장 중요합니다.

```c
#include "console_c.h"
#include <stdbool.h>

int main(void) {
    // 1. 시스템 초기화 (Raw Mode 진입, 입력 스레드 시작)
    cc_device_init();

    bool is_running = true;

    // 2. 메인 루프
    while (is_running) {
        // 여기에 로직이 들어갑니다.
        
        // 임시 탈출구 (무한루프 방지)
        // 실제로는 입력 처리에서 제어해야 합니다.
        if (cc_device_get_input(10) == CC_KEY_q) {
            is_running = false;
        }
    }

    // 3. 리소스 정리 및 터미널 복구
    cc_device_deinit();
    
    return 0;
}

```

---

## Step 2: 더블 버퍼링 & 렌더링 루프

깜빡임 없는 화면을 위해 `cc_buffer_t`를 사용합니다. 화면의 크기를 가져와(`get_size`) 버퍼를 맞추고(`resize`), 내용을 그린 뒤(`draw`), 최종적으로 출력(`flush`)합니다.

```c
    // ... (Init 이후)
    
    // 초기 버퍼 생성 (크기는 루프 내에서 맞춰짐)
    cc_buffer_t* buffer = cc_buffer_create(80, 24);

    while (is_running) {
        // 1. 현재 터미널 크기 확인
        cc_term_size_t size = cc_screen_get_size();

        // 2. 버퍼 크기 동기화 및 초기화 (검은색 배경)
        cc_buffer_resize(buffer, size._cols, size._rows);
        cc_buffer_clear(buffer, &CC_COLOR_BLACK);

        // 3. 그리기 (Draw Operations)
        // (x=2, y=2 위치에 흰색 글자)
        cc_buffer_draw_string(buffer, 2, 2, "Hello, ConsoleC UI!", &CC_COLOR_WHITE, &CC_COLOR_BLACK);
        
        // (박스 그리기)
        cc_buffer_draw_box(buffer, 1, 1, 40, 5, &CC_COLOR_CYAN, &CC_COLOR_BLACK, false);

        // 4. 플러시 (변경된 부분만 실제 터미널로 전송)
        cc_buffer_flush(buffer);
        
        // CPU 점유율 방지 (약 60FPS)
        cc_device_get_input(16); 
    }

    // ... (Deinit 전에)
    cc_buffer_destroy(buffer);

```

---

## Step 3: 입력 처리 (키보드)

`cc_device_get_input()` 함수는 지정된 시간(ms)만큼 대기하며 입력을 받습니다. 리턴된 키 코드를 `switch` 문으로 분기하여 처리합니다.

```c
    // ... (메인 루프 내부)

    // 1. 입력 대기 (10ms)
    cc_key_code_e key = cc_device_get_input(10);

    if (key != CC_KEY_NONE) {
        // 2. 키 이벤트 상세 분석 (필요시)
        cc_input_event_t event;
        cc_device_inspect(key, &event);

        // 3. 로직 처리
        switch (key) {
            case CC_KEY_q: // 종료
            case CC_KEY_ESC:
                is_running = false;
                break;
            
            case CC_KEY_UP:
                // 커서 위로 이동 로직
                break;
                
            case CC_KEY_ENTER:
                // 선택 로직
                break;
        }
    }

```

---

## Step 4: 마우스 상호작용 추가

마우스 클릭이나 드래그를 감지하려면 `cc_device_enable_mouse(true)`를 호출해야 합니다.

```c
    // ... (Init 직후)
    cc_device_enable_mouse(true);

    // ... (입력 처리 루프)
    if (key == CC_KEY_MOUSE_EVENT) {
        cc_input_event_t evt;
        cc_device_inspect(key, &evt);
        
        cc_mouse_state_t mouse = evt._data._mouse;
        
        if (mouse._button == CC_MOUSE_BTN_LEFT && mouse._action == CC_MOUSE_ACTION_PRESS) {
            // 클릭 처리: (mx, my) 좌표가 버튼 영역 안에 있는지 확인
            // if ( mx >= btn_x && mx < btn_x + btn_w ... )
        }
    }

```

---

## Step 5: 앱 구조화 (State Pattern)

코드 복잡도가 올라가면 `main` 함수가 너무 길어집니다. 이를 방지하기 위해 **상태 구조체(`app_state_t`)** 패턴을 사용하세요.

### 5.1 구조체 정의

```c
typedef struct {
    bool         _is_running;
    cc_buffer_t* _buffer;
    int          _cursor_x;
    int          _cursor_y;
    cc_color_t   _theme_color;
} app_state_t;

```

### 5.2 함수 분리

```c
void app_init(app_state_t* app) {
    memset(app, 0, sizeof(app_state_t));
    app->_is_running = true;
    app->_buffer = cc_buffer_create(80, 24);
    cc_device_init();
}

// Device 관련 (콘솔로 들어오는 모든 입력) 처리 부분
void app_handle_input(app_state_t* app) {
    cc_key_code_e key = cc_device_get_input(10);
    if (key == CC_KEY_q) app->_is_running = false;
    // ... 기타 입력 처리
}

// Screen 관련 (콘솔에 출력하는 모든 출력) 처리 부분
// cc_buffer 에다가 데이터를 쓴 다음에,
// cc_buffer_flush 를 이용해 실제 콘솔에 화면을 그립니다.
void app_render(app_state_t* app) {
    cc_term_size_t size = cc_screen_get_size();
    cc_buffer_resize(app->_buffer, size._cols, size._rows);
    cc_buffer_clear(app->_buffer, &CC_COLOR_BLACK);
    
    // UI 그리기 로직 함수 호출
    // draw_sidebar(app);
    // draw_content(app);
    
    cc_buffer_flush(app->_buffer);
}

void app_cleanup(app_state_t* app) {
    cc_buffer_destroy(app->_buffer);
    cc_device_deinit();
}

```

### 5.3 깔끔해진 Main

```c
int main(void) {
    app_state_t app;
    app_init(&app);

    while (app._is_running) {
        app_handle_input(&app);
        app_render(&app);
    }

    app_cleanup(&app);
    return 0;
}

```

---

## 🎨 UI 팁 (Tips)

1. **UTF-8 정렬:** 한글이 포함된 문자열을 출력할 때 UI가 깨지지 않으려면 `cc_util_get_string_width()`를 사용하여 실제 출력 너비를 계산하고, 남는 공간을 공백으로 채워야 합니다.
2. **색상 재사용:** `cc_color_t` 변수를 매번 `init` 하지 말고, `const cc_color_t* white = &CC_COLOR_WHITE;` 처럼 포인터를 활용하거나, 앱 상태 구조체에 미리 정의해두면 성능에 유리합니다.
3. **반응형 UI:** 모든 좌표 계산 시 `cc_screen_get_size()`로 얻은 `_cols`, `_rows`를 기준으로 상대 좌표(예: `width / 2`)를 사용하면 창 크기를 조절해도 UI가 유지됩니다.