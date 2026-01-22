#ifndef _CONSOLE_C_DEVICE_H_
#define _CONSOLE_C_DEVICE_H_

/** ------------------------------------------------------------------------------------
 * ConsoleC Device Input Module Header
 * ------------------------------------------------------------------------------------
 * 키보드/마우스 입력 처리, 시그널 핸들링, 스레드 안전한 입력 대기열 관리를 담당합니다.
 * cc_screen.h에 정의된 좌표/크기 타입을 사용하므로 cc_screen.h에 의존성을 가집니다.
 * ------------------------------------------------------------------------------------ */

#include "console_c/cc_screen.h" // [의존성 수정] cc_coord_t, cc_term_size_t 사용
#include <stdint.h>
#include <stdbool.h>

// -----------------------------------------------------------------------------
// Input Codes & Enums
// -----------------------------------------------------------------------------

/**
 * @brief 통합 입력 코드 (키보드, 마우스, 시스템 이벤트)
 */
typedef enum
{
    // --- Meta Signals ---
    CC_KEY_NONE      = -1,   /**< 입력 없음 / 타임아웃 */
    CC_KEY_INTERRUPT = -2,   /**< ForcePause / Signal에 의한 중단 */
    CC_KEY_BUSY      = -3,   /**< 다른 스레드가 이미 입력을 점유 중임 */

    // --- Events ---
    CC_KEY_MOUSE_EVENT  = 2000, /**< 마우스 동작 */
    CC_KEY_RESIZE_EVENT = 3000, /**< 터미널 크기 변경 (SIGWINCH) */
    CC_KEY_CURSOR_EVENT = 4000, /**< 커서 위치 응답 (내부 처리용) */

    // --- Standard Keys ---
    CC_KEY_TAB       = 9,
    CC_KEY_ENTER     = 10,
    CC_KEY_ESC       = 27,
    CC_KEY_SPACE     = 32,
    CC_KEY_BACKSPACE = 127,

    // --- Numbers (ASCII) ---
    CC_KEY_0 = 48, CC_KEY_1 = 49, CC_KEY_2 = 50, CC_KEY_3 = 51, CC_KEY_4 = 52,
    CC_KEY_5 = 53, CC_KEY_6 = 54, CC_KEY_7 = 55, CC_KEY_8 = 56, CC_KEY_9 = 57,

    // --- Uppercase Alphabets (ASCII) ---
    CC_KEY_A = 65, CC_KEY_B = 66, CC_KEY_C = 67, CC_KEY_D = 68, CC_KEY_E = 69,
    CC_KEY_F = 70, CC_KEY_G = 71, CC_KEY_H = 72, CC_KEY_I = 73, CC_KEY_J = 74,
    CC_KEY_K = 75, CC_KEY_L = 76, CC_KEY_M = 77, CC_KEY_N = 78, CC_KEY_O = 79,
    CC_KEY_P = 80, CC_KEY_Q = 81, CC_KEY_R = 82, CC_KEY_S = 83, CC_KEY_T = 84,
    CC_KEY_U = 85, CC_KEY_V = 86, CC_KEY_W = 87, CC_KEY_X = 88, CC_KEY_Y = 89,
    CC_KEY_Z = 90,

    // --- Lowercase Alphabets (ASCII) ---
    CC_KEY_a = 97,  CC_KEY_b = 98,  CC_KEY_c = 99,  CC_KEY_d = 100, CC_KEY_e = 101,
    CC_KEY_f = 102, CC_KEY_g = 103, CC_KEY_h = 104, CC_KEY_i = 105, CC_KEY_j = 106,
    CC_KEY_k = 107, CC_KEY_l = 108, CC_KEY_m = 109, CC_KEY_n = 110, CC_KEY_o = 111,
    CC_KEY_p = 112, CC_KEY_q = 113, CC_KEY_r = 114, CC_KEY_s = 115, CC_KEY_t = 116,
    CC_KEY_u = 117, CC_KEY_v = 118, CC_KEY_w = 119, CC_KEY_x = 120, CC_KEY_y = 121,
    CC_KEY_z = 122,

    // --- Special Keys ---
    CC_KEY_UP = 1001, CC_KEY_DOWN, CC_KEY_RIGHT, CC_KEY_LEFT,
    CC_KEY_INSERT = 1005, CC_KEY_DEL, CC_KEY_HOME, CC_KEY_END, CC_KEY_PAGE_UP, CC_KEY_PAGE_DOWN,

    // --- Function Keys ---
    CC_KEY_F1 = 1011, CC_KEY_F2, CC_KEY_F3, CC_KEY_F4, CC_KEY_F5, CC_KEY_F6,
    CC_KEY_F7, CC_KEY_F8, CC_KEY_F9, CC_KEY_F10, CC_KEY_F11, CC_KEY_F12

} cc_key_code_e;

typedef enum { CC_MOUSE_BTN_LEFT, CC_MOUSE_BTN_MIDDLE, CC_MOUSE_BTN_RIGHT, CC_MOUSE_BTN_UNKNOWN } cc_mouse_btn_e;
typedef enum { CC_MOUSE_ACTION_PRESS, CC_MOUSE_ACTION_DRAG, CC_MOUSE_ACTION_RELEASE, CC_MOUSE_ACTION_WHEEL_UP, CC_MOUSE_ACTION_WHEEL_DOWN, CC_MOUSE_ACTION_UNKNOWN } cc_mouse_action_e;

/**
 * @brief 마우스 상태 구조체
 */
typedef struct
{
    int               _x;
    int               _y;
    cc_mouse_btn_e    _button;
    cc_mouse_action_e _action;
} cc_mouse_state_t;

/**
 * @brief 통합 이벤트 구조체 (Tagged Union)
 * @details cc_screen.h 의 cc_term_size_t, cc_coord_t 타입을 활용합니다.
 */
typedef struct
{
    cc_key_code_e _code; /**< 이벤트 종류 (가장 먼저 확인) */

    union
    {
        cc_mouse_state_t _mouse;     /**< Valid if _code == CC_KEY_MOUSE_EVENT */
        cc_term_size_t   _term_size; /**< Valid if _code == CC_KEY_RESIZE_EVENT */
        cc_coord_t       _cursor;    /**< Valid if _code == CC_KEY_CURSOR_EVENT */
    } _data;

} cc_input_event_t;

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------

/**
 * @brief 디바이스 모듈을 초기화합니다. (Raw Mode 진입, 시그널 핸들러 등록)
 * @warning 프로그램 시작 시 1회 호출해야 합니다.
 */
void cc_device_init( void );

/**
 * @brief 디바이스 모듈을 종료하고 터미널 설정을 복구합니다.
 */
void cc_device_deinit( void );

/**
 * @brief [Blocking] 지정된 시간(ms) 동안 입력을 대기합니다.
 * @param timeout_ms 대기 시간 (음수: 무한 대기, 0: Non-blocking, 양수: 밀리초)
 * @return 입력 코드 (타임아웃 시 CC_KEY_NONE)
 */
cc_key_code_e cc_device_get_input( int timeout_ms );

/**
 * @brief 입력 코드를 상세 이벤트 객체로 변환합니다.
 * @param key_code cc_device_get_input()의 반환값
 * @param out_event [Output] 변환된 상세 이벤트 정보
 */
void cc_device_inspect( cc_key_code_e key_code, cc_input_event_t* out_event );

/**
 * @brief [Thread-Safe] 현재 커서 위치를 동기적으로 요청하여 반환합니다.
 * @param timeout_ms 타임아웃 시간
 * @param out_coord [Output] 커서 좌표
 * @return 성공 여부 (false: 타임아웃 혹은 실패)
 */
bool cc_device_get_cursor_pos( int timeout_ms, cc_coord_t* out_coord );

/**
 * @brief 마우스 추적 모드를 활성화/비활성화합니다.
 */
void cc_device_enable_mouse( bool enable );

/**
 * @brief 현재 마우스 상태를 반환합니다.
 */
cc_mouse_state_t cc_device_get_mouse_state( void );

/**
 * @brief Raw Mode 일시 해제 (Shell 명령어 실행 전 등)
 */
void cc_device_force_pause( void );

/**
 * @brief Raw Mode 재진입
 */
void cc_device_resume( void );

/**
 * @brief 키 코드를 문자열 설명으로 변환합니다. (디버깅용)
 */
const char* cc_device_key_to_string( cc_key_code_e key );

/**
 * @brief 숫자 키 입력을 실제 숫자로 변경합니다.
 */
int cc_key_to_int( cc_key_code_e key );

#endif // _CONSOLE_C_DEVICE_H_