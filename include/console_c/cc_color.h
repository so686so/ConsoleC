#ifndef _CONSOLE_C_COLOR_H_
#define _CONSOLE_C_COLOR_H_

/** ------------------------------------------------------------------------------------
 * ConsoleC Color Module Header
 * ------------------------------------------------------------------------------------
 * RGB 색상 데이터를 관리하고 ANSI Escape Code 변환을 담당합니다.
 * C11 표준 및 Caller-Allocated Buffer 전략을 따릅니다.
 * ------------------------------------------------------------------------------------ */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief ANSI 코드 및 Hex 문자열 변환 시 필요한 안전한 버퍼 크기
 * @details "\033[38;2;255;255;255m" 형태는 약 20바이트 내외이므로 32바이트면 충분함.
 */
#define CC_COLOR_FMT_BUF_SIZE 32

/**
 * @brief 색상 타입 열거형
 */
typedef enum
{
    CC_COLOR_TYPE_NONE  = 0,
    CC_COLOR_TYPE_RGB   = 1,
    CC_COLOR_TYPE_RESET = 2
} cc_color_type_e;

/**
 * @brief RGB 값을 담는 단순 구조체
 */
typedef struct
{
    uint8_t _r;
    uint8_t _g;
    uint8_t _b;
} cc_rgb_t;

/**
 * @brief 색상 관리 구조체
 */
typedef struct
{
    cc_color_type_e _type;
    cc_rgb_t        _rgb;
} cc_color_t;

// -----------------------------------------------------------------------------
// Global Presets (Defined in cc_color.c)
// -----------------------------------------------------------------------------
extern const cc_color_t CC_COLOR_BLACK;
extern const cc_color_t CC_COLOR_WHITE;
extern const cc_color_t CC_COLOR_RED;
extern const cc_color_t CC_COLOR_GREEN;
extern const cc_color_t CC_COLOR_BLUE;
extern const cc_color_t CC_COLOR_YELLOW;
extern const cc_color_t CC_COLOR_CYAN;
extern const cc_color_t CC_COLOR_MAGENTA;
extern const cc_color_t CC_COLOR_GRAY;
extern const cc_color_t CC_COLOR_RESET; /**< 터미널 색상 초기화 (\033[0m) */

// -----------------------------------------------------------------------------
// Function Prototypes
// -----------------------------------------------------------------------------

/**
 * @brief RGB 값으로 색상 객체를 초기화합니다.
 * @param out_color 초기화할 구조체 포인터
 * @param r Red (0~255)
 * @param g Green (0~255)
 * @param b Blue (0~255)
 */
void cc_color_init_rgb( cc_color_t* out_color, uint8_t r, uint8_t g, uint8_t b );

/**
 * @brief Hex 문자열로 색상 객체를 초기화합니다.
 * @param out_color 초기화할 구조체 포인터
 * @param hex_code Hex 문자열 (예: "#FF0000" or "FF0000")
 * @return 파싱 성공 여부 (실패 시 CC_COLOR_TYPE_NONE 상태가 됨)
 */
bool cc_color_init_hex( cc_color_t* out_color, const char* hex_code );

/**
 * @brief 특수 타입(RESET, NONE 등)으로 초기화합니다.
 * @param out_color 초기화할 구조체 포인터
 * @param type 색상 타입
 */
void cc_color_init_type( cc_color_t* out_color, cc_color_type_e type );

/**
 * @brief 전경색(글자색)용 ANSI 시퀀스를 버퍼에 작성합니다.
 * @param self 대상 색상 객체
 * @param buf 결과를 저장할 버퍼 (최소 CC_COLOR_FMT_BUF_SIZE 권장)
 * @param buf_len 버퍼의 크기
 * @return 작성된 문자열의 포인터 (buf와 동일), 실패 시 NULL
 */
const char* cc_color_to_ansi_fg( const cc_color_t* self, char* buf, size_t buf_len );

/**
 * @brief 배경색용 ANSI 시퀀스를 버퍼에 작성합니다.
 * @param self 대상 색상 객체
 * @param buf 결과를 저장할 버퍼 (최소 CC_COLOR_FMT_BUF_SIZE 권장)
 * @param buf_len 버퍼의 크기
 * @return 작성된 문자열의 포인터 (buf와 동일), 실패 시 NULL
 */
const char* cc_color_to_ansi_bg( const cc_color_t* self, char* buf, size_t buf_len );

/**
 * @brief Hex 문자열(예: "#RRGGBB")을 버퍼에 작성합니다.
 * @param self 대상 색상 객체
 * @param buf 결과를 저장할 버퍼 (최소 8바이트 이상 필요)
 * @param buf_len 버퍼의 크기
 * @return 작성된 문자열의 포인터, 실패 시 NULL
 */
const char* cc_color_to_hex( const cc_color_t* self, char* buf, size_t buf_len );

/**
 * @brief 두 색상 객체가 동일한지 비교합니다.
 */
bool cc_color_is_equal( const cc_color_t* lhs, const cc_color_t* rhs );

/**
 * @brief 유효한 색상인지 확인합니다. (NONE이 아닌 경우)
 */
bool cc_color_is_valid( const cc_color_t* self );

/**
 * @brief RGB 타입인지 확인합니다.
 */
bool cc_color_is_rgb( const cc_color_t* self );

#endif // _CONSOLE_C_COLOR_H_