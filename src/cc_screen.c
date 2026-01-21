/** ------------------------------------------------------------------------------------
 * ConsoleC Screen Module Implementation
 * ------------------------------------------------------------------------------------
 * cc_screen.h 의 구현부입니다.
 * ------------------------------------------------------------------------------------ */

#include "console_c/cc_screen.h"

#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>

// -----------------------------------------------------------------------------
// Global Constants
// -----------------------------------------------------------------------------
const cc_coord_t CC_COORD_ZERO   = { 0, 0 };
const cc_coord_t CC_COORD_ORIGIN = { 1, 1 };

// -----------------------------------------------------------------------------
// Internal Helpers
// -----------------------------------------------------------------------------

static int _clamp( int v, int min, int max )
{
    if( v < min ) return min;
    if( v > max ) return max;
    return v;
}

static cc_coord_t _clamp_to_terminal( cc_coord_t pos )
{
    cc_term_size_t size = cc_screen_get_size();

    // 터미널 크기를 못 가져왔거나 0일 경우에 대한 방어 코드
    int max_w = ( size._cols > 0 ) ? size._cols : 999;
    int max_h = ( size._rows > 0 ) ? size._rows : 999;

    cc_coord_t safe_pos;
    safe_pos._x = _clamp( pos._x, 1, max_w );
    safe_pos._y = _clamp( pos._y, 1, max_h );

    return safe_pos;
}

// -----------------------------------------------------------------------------
// Public API Implementation
// -----------------------------------------------------------------------------

cc_term_size_t cc_screen_get_size( void )
{
    struct winsize ws;

    // STDOUT_FILENO(표준 출력)의 윈도우 사이즈 정보를 가져옵니다.
    if( ioctl( STDOUT_FILENO, TIOCGWINSZ, &ws ) == -1 ){
        cc_term_size_t empty = { 0, 0 };
        return empty;
    }

    cc_term_size_t size;
    size._cols = ws.ws_col;
    size._rows = ws.ws_row;
    
    return size;
}

bool cc_screen_move_cursor( int x, int y )
{
    cc_coord_t pos = { x, y };
    return cc_screen_move_cursor_v( pos );
}

bool cc_screen_move_cursor_v( cc_coord_t pos )
{
    if( pos._x <= 0 || pos._y <= 0 ){
        return false;
    }

    cc_coord_t safe_pos = _clamp_to_terminal( pos );

    // ANSI: \033[<row>;<col>H  (y, x 순서 주의)
    printf( "\033[%d;%dH", safe_pos._y, safe_pos._x );
    
    // 즉시 반영을 위해 flush (필요에 따라 생략 가능하나 안전하게)
    // fflush( stdout ); 
    // -> 보통 출력 모듈들은 버퍼링을 하다가 한 번에 쏘는 게 성능에 좋지만,
    //    이 API는 단독 커서 제어 명령이므로 fflush를 하는 것이 일반적입니다.
    //    단, cc_buffer 등 상위 모듈에서 Flush 할 때는 이 함수를 안 쓰고 직접 버퍼링 하므로 상관 없음.
    
    return true;
}

void cc_screen_move_cursor_relative( int dx, int dy )
{
    if( dx == 0 && dy == 0 ) return;

    if( dy < 0 ) printf( "\033[%dA", -dy ); // Up
    if( dy > 0 ) printf( "\033[%dB",  dy ); // Down
    if( dx > 0 ) printf( "\033[%dC",  dx ); // Right
    if( dx < 0 ) printf( "\033[%dD", -dx ); // Left
}

void cc_screen_clear( void )
{
    // 2J: 화면 전체 지우기, 1;1H: 커서 홈 이동
    printf( "\033[2J\033[1;1H" );
    fflush( stdout );
}

bool cc_screen_set_color( const cc_color_t* color )
{
    if( !cc_color_is_valid( color ) ){
        return false;
    }

    char buf[64];
    if( cc_color_to_ansi_fg( color, buf, sizeof(buf) ) ){
        printf( "%s", buf );
        return true;
    }
    return false;
}

bool cc_screen_set_back_color( const cc_color_t* color )
{
    if( !cc_color_is_valid( color ) ){
        return false;
    }

    char buf[64];
    if( cc_color_to_ansi_bg( color, buf, sizeof(buf) ) ){
        printf( "%s", buf );
        return true;
    }
    return false;
}

void cc_screen_reset_color( void )
{
    char buf[64];
    // CC_COLOR_RESET은 extern const이므로 주소 전달
    if( cc_color_to_ansi_fg( &CC_COLOR_RESET, buf, sizeof(buf) ) ){
        printf( "%s", buf );
    }
}

// -----------------------------------------------------------------------------
// Utilities Implementation
// -----------------------------------------------------------------------------

bool cc_coord_is_equal( cc_coord_t a, cc_coord_t b )
{
    return ( a._x == b._x ) && ( a._y == b._y );
}

cc_coord_t cc_coord_add( cc_coord_t a, cc_coord_t b )
{
    cc_coord_t res;
    res._x = a._x + b._x;
    res._y = a._y + b._y;
    return res;
}