#ifndef _CONSOLE_C_SCREEN_H_
#define _CONSOLE_C_SCREEN_H_

/** ------------------------------------------------------------------------------------
 * ConsoleC Screen Module Header
 * ------------------------------------------------------------------------------------
 * 콘솔 화면의 좌표계 정의, 터미널 크기 조회, 커서 이동,
 * 색상 출력 제어 및 화면 청소 기능을 담당합니다.
 * ------------------------------------------------------------------------------------ */

#include "console_c/cc_color.h"
#include <stdbool.h>

// -----------------------------------------------------------------------------
// Geometry Types (Defined here as the base)
// -----------------------------------------------------------------------------

/**
 * @brief 2D 좌표 구조체 (1-based Index)
 */
typedef struct
{
    int _x; /**< Column (가로) */
    int _y; /**< Row    (세로) */
} cc_coord_t;

/**
 * @brief 터미널 화면 크기 구조체
 */
typedef struct
{
    int _cols; /**< 너비 (Width) */
    int _rows; /**< 높이 (Height) */
} cc_term_size_t;

// -----------------------------------------------------------------------------
// Constants
// -----------------------------------------------------------------------------
extern const cc_coord_t CC_COORD_ZERO;   /**< (0, 0) */
extern const cc_coord_t CC_COORD_ORIGIN; /**< (1, 1) */

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------

/**
 * @brief 현재 터미널 창의 크기(Col, Row)를 조회합니다.
 * @return 너비와 높이 정보 (실패 시 0,0)
 */
cc_term_size_t cc_screen_get_size( void );

/**
 * @brief 콘솔 커서를 절대 좌표로 이동시킵니다.
 * @param x 이동할 X 좌표 (Column, 1-based)
 * @param y 이동할 Y 좌표 (Row, 1-based)
 * @return 성공 여부 (화면 밖 좌표는 보정되어 이동 후 true 반환, 좌표가 음수면 false)
 */
bool cc_screen_move_cursor( int x, int y );

/**
 * @brief 콘솔 커서를 Coord 구조체를 이용해 이동시킵니다.
 */
bool cc_screen_move_cursor_v( cc_coord_t pos );

/**
 * @brief 콘솔 커서를 현재 위치 기준으로 상대 이동시킵니다.
 * @param dx 가로 이동량 (+: 오른쪽, -: 왼쪽)
 * @param dy 세로 이동량 (+: 아래쪽, -: 위쪽)
 */
void cc_screen_move_cursor_relative( int dx, int dy );

/**
 * @brief 화면 전체를 지우고 커서를 (1,1)로 초기화합니다.
 */
void cc_screen_clear( void );

/**
 * @brief 전경색(글자색)을 설정합니다.
 */
bool cc_screen_set_color( const cc_color_t* color );

/**
 * @brief 배경색을 설정합니다.
 */
bool cc_screen_set_back_color( const cc_color_t* color );

/**
 * @brief 글자색과 배경색을 모두 터미널 기본값으로 초기화합니다.
 */
void cc_screen_reset_color( void );

// -----------------------------------------------------------------------------
// Utilities
// -----------------------------------------------------------------------------

/**
 * @brief 두 좌표가 같은지 비교합니다.
 */
bool cc_coord_is_equal( cc_coord_t a, cc_coord_t b );

/**
 * @brief 좌표 덧셈 (a + b)
 */
cc_coord_t cc_coord_add( cc_coord_t a, cc_coord_t b );

#endif // _CONSOLE_C_SCREEN_H_