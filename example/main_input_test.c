/** ------------------------------------------------------------------------------------
 * ConsoleC Example: Input Test
 * ------------------------------------------------------------------------------------
 * F1: 실시간 입력 모드 (마우스 클릭/드래그 시 마커 표시)
 * F2: 라인 입력 모드 (엔터 시 입력된 문장 반환)
 * F3: 마커 초기화
 * ------------------------------------------------------------------------------------ */

#define _POSIX_C_SOURCE 200809L

#include "console_c.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <ctype.h>
#include <unistd.h>

// -----------------------------------------------------------------------------
// Constants & Types
// -----------------------------------------------------------------------------

#define MAX_LOGS 10
#define MAX_MARKERS 1000
#define MAX_LINE_BUF 128

typedef enum {
    MODE_REALTIME, // F1: 즉시 반환 및 마우스 처리
    MODE_LINE      // F2: 문장 입력 (Buffer)
} input_mode_e;

typedef struct {
    char _msg[128];
    char _time_str[16];
} log_entry_t;

typedef struct {
    int _x;
    int _y;
} marker_t;

typedef struct {
    bool            _is_running;
    input_mode_e    _mode;

    // Resources
    cc_buffer_t* _buffer;

    // Logs (Latest at index 0)
    log_entry_t     _logs[MAX_LOGS];
    int             _log_count;

    // Line Input (F2 Mode)
    char            _line_buf[MAX_LINE_BUF];
    int             _line_len;

    // Markers (Mouse Click/Drag)
    marker_t        _markers[MAX_MARKERS];
    int             _marker_count;

    // Colors (Optimization)
    cc_color_t      _c_white;
    cc_color_t      _c_black;
    cc_color_t      _c_cyan;
    cc_color_t      _c_gray;
    cc_color_t      _c_yellow;
    cc_color_t      _c_green;
} app_state_t;

// -----------------------------------------------------------------------------
// Helper Functions
// -----------------------------------------------------------------------------

// 현재 시간을 문자열로 반환
static void _get_current_time(char* buf, size_t size) {
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    strftime(buf, size, "%H:%M:%S", t);
}

// 로그 추가 (최신순 정렬을 위해 배열 밀기)
static void _add_log(app_state_t* app, const char* fmt, ...) {
    // Shift logs down
    if (app->_log_count < MAX_LOGS) {
        app->_log_count++;
    }
    for (int i = app->_log_count - 1; i > 0; --i) {
        app->_logs[i] = app->_logs[i - 1];
    }

    // New log at index 0
    _get_current_time(app->_logs[0]._time_str, sizeof(app->_logs[0]._time_str));

    va_list args;
    va_start(args, fmt);
    vsnprintf(app->_logs[0]._msg, sizeof(app->_logs[0]._msg), fmt, args);
    va_end(args);
}

// 마커 추가
static void _add_marker(app_state_t* app, int x, int y) {
    if (app->_marker_count < MAX_MARKERS) {
        app->_markers[app->_marker_count]._x = x;
        app->_markers[app->_marker_count]._y = y;
        app->_marker_count++;
    }
}

// 마커 클리어
static void _clear_markers(app_state_t* app) {
    app->_marker_count = 0;
    _add_log(app, "[System] Markers Cleared (F3)");
}

// -----------------------------------------------------------------------------
// Input Handling
// -----------------------------------------------------------------------------

static void _handle_realtime_input(app_state_t* app, cc_key_code_e key, const cc_input_event_t* evt) {
    if (key == CC_KEY_MOUSE_EVENT) {
        const cc_mouse_state_t* m = &evt->_data._mouse;

        // 클릭 혹은 드래그 시
        if (m->_button == CC_MOUSE_BTN_LEFT &&
           (m->_action == CC_MOUSE_ACTION_PRESS || m->_action == CC_MOUSE_ACTION_DRAG)) {

            _add_marker(app, m->_x, m->_y);

            const char* act_str = (m->_action == CC_MOUSE_ACTION_PRESS) ? "Click" : "Drag";
            _add_log(app, "[Mouse] %s at (%d, %d)", act_str, m->_x, m->_y);
        }
    }
    else {
        // 일반 키 입력 즉시 로깅
        const char* key_name = cc_device_key_to_string(key);
        _add_log(app, "[Key] Immediate: %s (%d)", key_name, (int)key);
    }
}

static void _handle_line_input(app_state_t* app, cc_key_code_e key) {
    if (key == CC_KEY_ENTER) {
        // 엔터: 입력 버퍼 제출
        if (app->_line_len > 0) {
            _add_log(app, "[Line] Result: %s", app->_line_buf);
            app->_line_buf[0] = '\0';
            app->_line_len = 0;
        }
    }
    else if (key == CC_KEY_BACKSPACE) {
        // 백스페이스
        if (app->_line_len > 0) {
            app->_line_buf[--app->_line_len] = '\0';
        }
    }
    else if (key >= CC_KEY_SPACE && key <= 126) {
        // 출력 가능한 문자 (ASCII)
        if (app->_line_len < MAX_LINE_BUF - 1) {
            app->_line_buf[app->_line_len++] = (char)key;
            app->_line_buf[app->_line_len] = '\0';
        }
    }
}

static void _process_input(app_state_t* app) {
    // 10ms 대기
    cc_key_code_e key = cc_device_get_input(10);

    if (key == CC_KEY_NONE) return;

    cc_input_event_t evt;
    cc_device_inspect(key, &evt);

    // 공통 기능키
    if (key == CC_KEY_ESC) {
        app->_is_running = false;
        return;
    }
    if (key == CC_KEY_F1) {
        app->_mode = MODE_REALTIME;
        _add_log(app, "[Mode] Switched to Realtime (F1)");
        return;
    }
    if (key == CC_KEY_F2) {
        app->_mode = MODE_LINE;
        app->_line_buf[0] = '\0'; // 버퍼 초기화
        app->_line_len = 0;
        _add_log(app, "[Mode] Switched to Line Input (F2)");
        return;
    }
    if (key == CC_KEY_F3) {
        _clear_markers(app);
        return;
    }

    // 모드별 처리
    if (app->_mode == MODE_REALTIME) {
        _handle_realtime_input(app, key, &evt);
    } else {
        // 라인 모드에서는 마우스 무시
        if (key != CC_KEY_MOUSE_EVENT && key != CC_KEY_RESIZE_EVENT) {
            _handle_line_input(app, key);
        }
    }
}

// -----------------------------------------------------------------------------
// Rendering
// -----------------------------------------------------------------------------

static void _draw_ui(app_state_t* app) {
    cc_term_size_t size = cc_screen_get_size();

    // 1. 배경 클리어
    cc_buffer_resize(app->_buffer, size._cols, size._rows);
    cc_buffer_clear(app->_buffer, &app->_c_black);

    // 2. 상단 상태바
    char status_bar[128];
    const char* mode_str = (app->_mode == MODE_REALTIME) ? "REALTIME" : "LINE INPUT";
    cc_color_t mode_color = (app->_mode == MODE_REALTIME) ? app->_c_cyan : app->_c_green;

    snprintf(status_bar, sizeof(status_bar),
        " [F1] Realtime | [F2] Line Input | [F3] Clear | [ESC] Quit ");

    cc_buffer_draw_box(app->_buffer, 0, 0, size._cols, 3, &app->_c_white, &app->_c_black, false);
    cc_buffer_draw_string(app->_buffer, 2, 1, status_bar, &app->_c_white, &app->_c_black);

    char mode_disp[64];
    snprintf(mode_disp, sizeof(mode_disp), " MODE: %s ", mode_str);
    int mode_x = size._cols - (int)strlen(mode_disp) - 2;
    if (mode_x > 50) {
        cc_buffer_draw_string(app->_buffer, mode_x, 2, mode_disp, &mode_color, &app->_c_black);
    }

    // 3. 로그 박스 (하단)
    int log_h = 10;
    int log_y = size._rows - log_h + 1;

    cc_buffer_draw_box(app->_buffer, 1, log_y, size._cols, log_h, &app->_c_gray, &app->_c_black, false);
    cc_buffer_draw_string(app->_buffer, 3, log_y, " [ Input Logs ] ", &app->_c_yellow, &app->_c_black);

    for (int i = 0; i < app->_log_count; ++i) {
        int row = log_y + 2 + i;
        if (row >= size._rows - 1) break;

        char line[256];
        snprintf(line, sizeof(line), "[%s] %s", app->_logs[i]._time_str, app->_logs[i]._msg);

        const cc_color_t* col = (i == 0) ? &app->_c_white : &app->_c_gray;
        cc_buffer_draw_string(app->_buffer, 3, row, line, col, &app->_c_black);
    }

    // 4. 입력 상태 표시
    if (app->_mode == MODE_LINE) {
        int input_y = log_y - 2;
        cc_buffer_draw_string(app->_buffer, 3, input_y, "INPUT > ", &app->_c_green, &app->_c_black);
        cc_buffer_draw_string(app->_buffer, 11, input_y, app->_line_buf, &app->_c_white, &app->_c_black);
        cc_buffer_draw_string(app->_buffer, 11 + app->_line_len, input_y, "_", &app->_c_green, &app->_c_black);
    }

    // 5. 마커 그리기
    for (int i = 0; i < app->_marker_count; ++i) {
        int x = app->_markers[i]._x;
        int y = app->_markers[i]._y;

        if (x < 0 || x >= size._cols || y < 0 || y >= size._rows) continue;

        // [최신 마커 Cyan, 나머지 Gray]
        bool is_latest = (i == app->_marker_count - 1);
        const cc_color_t* mark_col = is_latest ? &app->_c_cyan : &app->_c_gray;

        cc_buffer_draw_string(app->_buffer, x, y, "+", mark_col, &app->_c_black);
    }

    cc_buffer_flush(app->_buffer);
}

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------

void app_init(app_state_t* app) {
    memset(app, 0, sizeof(app_state_t));
    app->_is_running = true;
    app->_mode = MODE_REALTIME;
    app->_buffer = cc_buffer_create(80, 24);

    app->_c_white  = CC_COLOR_WHITE;
    app->_c_black  = CC_COLOR_BLACK;
    app->_c_cyan   = CC_COLOR_CYAN;
    app->_c_gray   = CC_COLOR_GRAY;
    app->_c_yellow = CC_COLOR_YELLOW;
    app->_c_green  = CC_COLOR_GREEN;

    cc_device_init();
    cc_device_enable_mouse(true);
    cc_screen_set_back_color(&CC_COLOR_BLACK);
    cc_screen_clear();
}

void app_cleanup(app_state_t* app) {
    cc_screen_set_color( &CC_COLOR_RESET );
    cc_device_enable_mouse(false);
    cc_screen_clear();
    cc_buffer_destroy(app->_buffer);
    cc_device_deinit();
}

int main(void) {
    app_state_t app;
    app_init(&app);

    while (app._is_running) {
        _process_input(&app);

        // [중요] ESC 누른 직후 불필요한 렌더링을 막고 즉시 종료
        if (!app._is_running) break;

        _draw_ui(&app);
    }

    app_cleanup(&app);
    return 0;
}