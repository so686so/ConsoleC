#ifndef _CONSOLE_C_BUFFER_H_
#define _CONSOLE_C_BUFFER_H_

/** ------------------------------------------------------------------------------------
 * ConsoleC Buffer Module Header
 * ------------------------------------------------------------------------------------
 * 더블 버퍼링(Double Buffering)을 지원하는 화면 렌더러입니다.
 * 2D 좌표를 1D 배열로 플랫하게 관리하여 성능을 최적화했습니다.
 * ------------------------------------------------------------------------------------ */

#include "console_c/cc_color.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 화면의 한 칸을 나타내는 구조체
 * @details 단일 UTF-8 문자를 저장하기 위해 고정된 크기의 char 배열을 사용합니다.
 */
typedef struct
{
    char       _ch[5];         /**< 출력할 문자 (UTF-8, Max 4bytes + Null) */
    cc_color_t _fg;            /**< 글자색 */
    cc_color_t _bg;            /**< 배경색 */
    bool       _is_wide_trail; /**< 2칸짜리 문자의 뒷부분인지 여부 */
} cc_cell_t;

/**
 * @brief 더블 버퍼링 관리 구조체
 */
typedef struct
{
    int        _width;        /**< 버퍼 너비 */
    int        _height;       /**< 버퍼 높이 */
    
    /**
     * @brief Front Buffer (현재 화면 상태)
     * @details 1차원 배열로 관리 (index = y * width + x)
     */
    cc_cell_t* _front_buffer; 

    /**
     * @brief Back Buffer (다음 프레임 상태)
     * @details 1차원 배열로 관리 (index = y * width + x)
     */
    cc_cell_t* _back_buffer;  
} cc_buffer_t;

// -----------------------------------------------------------------------------
// Function Prototypes
// -----------------------------------------------------------------------------

/**
 * @brief 버퍼 객체를 생성하고 메모리를 할당합니다.
 * @param width 초기 너비
 * @param height 초기 높이
 * @return 생성된 버퍼 객체 포인터 (실패 시 NULL)
 */
cc_buffer_t* cc_buffer_create( int width, int height );

/**
 * @brief 버퍼 객체를 메모리에서 해제합니다.
 * @param self 해제할 객체 포인터
 */
void cc_buffer_destroy( cc_buffer_t* self );

/**
 * @brief 버퍼 크기를 조절합니다. (화면 리사이즈 시 호출)
 * @param self 대상 객체
 * @param width 변경할 너비
 * @param height 변경할 높이
 * @return 성공 여부 (메모리 재할당 실패 시 false)
 */
bool cc_buffer_resize( cc_buffer_t* self, int width, int height );

/**
 * @brief Back Buffer를 특정 배경색으로 초기화합니다. (매 프레임 시작 시 호출)
 * @param self 대상 객체
 * @param bg_color 채울 배경색
 */
void cc_buffer_clear( cc_buffer_t* self, const cc_color_t* bg_color );

/**
 * @brief 문자열을 특정 좌표에 그립니다.
 * @param self 대상 객체
 * @param x X 좌표
 * @param y Y 좌표
 * @param text 출력할 문자열 (UTF-8)
 * @param fg 글자색
 * @param bg 배경색
 */
void cc_buffer_draw_string( cc_buffer_t* self, int x, int y, const char* text, const cc_color_t* fg, const cc_color_t* bg );

/**
 * @brief UI 테두리용 박스를 그립니다.
 * @param self 대상 객체
 * @param x 시작 X 좌표
 * @param y 시작 Y 좌표
 * @param w 박스 너비
 * @param h 박스 높이
 * @param fg 선 색상
 * @param bg 배경색
 * @param red_border true일 경우 테두리를 빨간색으로 강제 (디버그/강조용)
 */
void cc_buffer_draw_box( cc_buffer_t* self, int x, int y, int w, int h, const cc_color_t* fg, const cc_color_t* bg, bool red_border );

/**
 * @brief [핵심] 변경된 부분(Diff)만 계산하여 터미널로 출력합니다.
 * @details Back Buffer와 Front Buffer를 비교하여 달라진 부분만 ANSI 코드로 출력하고,
 * Front Buffer를 Back Buffer 상태로 동기화합니다.
 * @param self 대상 객체
 */
void cc_buffer_flush( cc_buffer_t* self );

#endif // _CONSOLE_C_BUFFER_H_