/** ------------------------------------------------------------------------------------
 * ConsoleC Buffer Module Implementation
 * ------------------------------------------------------------------------------------
 * cc_buffer.h 의 구현부입니다.
 * ------------------------------------------------------------------------------------ */

#include "console_c/cc_buffer.h"
#include "console_c/cc_util.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// -----------------------------------------------------------------------------
// Internal Macros & Helpers
// -----------------------------------------------------------------------------

#define INDEX( _buf, _x, _y ) ( ( _y ) * ( _buf )->_width + ( _x ) )

/**
 * @brief UTF-8 첫 바이트를 보고 전체 바이트 길이를 반환
 */
static int _get_utf8_len( char c )
{
    if( ( c & 0x80 ) == 0 ) return 1;
    if( ( c & 0xE0 ) == 0xC0 ) return 2;
    if( ( c & 0xF0 ) == 0xE0 ) return 3;
    if( ( c & 0xF8 ) == 0xF0 ) return 4;
    return 1; // Fallback
}

/**
 * @brief 두 Cell이 동일한지 비교
 */
static bool _is_cell_equal( const cc_cell_t* lhs, const cc_cell_t* rhs )
{
    // 1. 문자열 비교 (고정 5바이트)
    if( strcmp( lhs->_ch, rhs->_ch ) != 0 ){
        return false;
    }
    // 2. 색상 비교
    if( !cc_color_is_equal( &lhs->_fg, &rhs->_fg ) ){
        return false;
    }
    if( !cc_color_is_equal( &lhs->_bg, &rhs->_bg ) ){
        return false;
    }
    // 3. Wide Trail 여부
    if( lhs->_is_wide_trail != rhs->_is_wide_trail ){
        return false;
    }
    return true;
}

/**
 * @brief 내부 버퍼 초기화 (Resize, Clear 등에서 사용)
 */
static void _fill_buffer( cc_cell_t* buffer, int count, const cc_color_t* bg )
{
    for( int i = 0; i < count; ++i ){
        strcpy( buffer[i]._ch, " " );
        buffer[i]._fg = CC_COLOR_WHITE;
        buffer[i]._bg = ( bg ) ? *bg : CC_COLOR_BLACK;
        buffer[i]._is_wide_trail = false;
    }
}

// -----------------------------------------------------------------------------
// Public API Implementation
// -----------------------------------------------------------------------------

cc_buffer_t* cc_buffer_create( int width, int height )
{
    if( width <= 0 || height <= 0 ) return NULL;

    cc_buffer_t* self = (cc_buffer_t*)malloc( sizeof( cc_buffer_t ) );
    if( !self ) return NULL;

    self->_width  = width;
    self->_height = height;

    size_t buf_size = sizeof( cc_cell_t ) * width * height;

    self->_front_buffer = (cc_cell_t*)malloc( buf_size );
    self->_back_buffer  = (cc_cell_t*)malloc( buf_size );

    if( !self->_front_buffer || !self->_back_buffer ){
        cc_buffer_destroy( self ); // cleanup partial allocation
        return NULL;
    }

    // 초기화
    _fill_buffer( self->_front_buffer, width * height, &CC_COLOR_BLACK );
    _fill_buffer( self->_back_buffer, width * height, &CC_COLOR_BLACK );

    return self;
}

void cc_buffer_destroy( cc_buffer_t* self )
{
    if( !self ) return;

    if( self->_front_buffer ) free( self->_front_buffer );
    if( self->_back_buffer )  free( self->_back_buffer );

    free( self );
}

bool cc_buffer_resize( cc_buffer_t* self, int width, int height )
{
    if( !self ) return false;
    if( self->_width == width && self->_height == height ) return true;

    // 기존 버퍼 해제 후 재할당 (단순 realloc보다 안전하게 초기화하기 위함)
    if( self->_front_buffer ) free( self->_front_buffer );
    if( self->_back_buffer )  free( self->_back_buffer );

    self->_width  = width;
    self->_height = height;

    size_t buf_size = sizeof( cc_cell_t ) * width * height;
    self->_front_buffer = (cc_cell_t*)malloc( buf_size );
    self->_back_buffer  = (cc_cell_t*)malloc( buf_size );

    if( !self->_front_buffer || !self->_back_buffer ){
        // 메모리 할당 실패 시 객체 상태가 불안정하므로 최소한 NULL 처리
        if( self->_front_buffer ) { free( self->_front_buffer ); self->_front_buffer = NULL; }
        if( self->_back_buffer )  { free( self->_back_buffer );  self->_back_buffer = NULL; }
        return false;
    }

    // 화면 전체 갱신을 위해 초기화
    _fill_buffer( self->_front_buffer, width * height, &CC_COLOR_BLACK );
    _fill_buffer( self->_back_buffer, width * height, &CC_COLOR_BLACK );

    return true;
}

void cc_buffer_clear( cc_buffer_t* self, const cc_color_t* bg_color )
{
    if( !self || !self->_back_buffer ) return;

    int count = self->_width * self->_height;
    _fill_buffer( self->_back_buffer, count, bg_color );
}

void cc_buffer_draw_string( cc_buffer_t* self, int x, int y, const char* text, const cc_color_t* fg, const cc_color_t* bg )
{
    if( !self || !text ) return;
    if( y < 0 || y >= self->_height ) return;

    int cursor_x = x;
    size_t i = 0;
    size_t len = strlen( text );

    // 안전한 디폴트 색상
    cc_color_t safe_fg = ( fg ) ? *fg : CC_COLOR_WHITE;
    cc_color_t safe_bg = ( bg ) ? *bg : CC_COLOR_BLACK;

    while( i < len && cursor_x < self->_width ){
        // 1. UTF-8 Byte Length
        int char_len = _get_utf8_len( text[i] );

        // 2. Visual Width Calculation
        // cc_util을 활용하여 코드포인트 추출 후 너비 계산
        // (단순화를 위해 char copy 후 검사)
        char temp_ch[5] = {0};
        if( char_len > 4 ) char_len = 1; // Defensive check
        memcpy( temp_ch, &text[i], char_len );
        temp_ch[char_len] = '\0';

        size_t visual_width = cc_util_get_string_width( temp_ch );

        // 3. Draw to Back Buffer
        if( cursor_x >= 0 && cursor_x < self->_width ){
            int idx = INDEX( self, cursor_x, y );
            cc_cell_t* cell = &self->_back_buffer[idx];

            strcpy( cell->_ch, temp_ch );
            cell->_fg = safe_fg;
            cell->_bg = safe_bg;
            cell->_is_wide_trail = false;

            // Wide char 처리 (한글 등 2칸 문자)
            if( visual_width == 2 && cursor_x + 1 < self->_width ){
                int next_idx = INDEX( self, cursor_x + 1, y );
                cc_cell_t* trail = &self->_back_buffer[next_idx];

                strcpy( trail->_ch, "" ); // 빈 문자
                trail->_fg = safe_fg;
                trail->_bg = safe_bg;
                trail->_is_wide_trail = true;
            }
        }

        cursor_x += (int)visual_width;
        i += char_len;
    }
}

void cc_buffer_draw_box( cc_buffer_t* self, int x, int y, int w, int h, const cc_color_t* fg, const cc_color_t* bg, bool red_border )
{
    if( !self ) return;

    cc_color_t border_fg = ( red_border ) ? CC_COLOR_RED : ( fg ? *fg : CC_COLOR_WHITE );
    cc_color_t safe_bg   = ( bg ) ? *bg : CC_COLOR_BLACK;

    // Corners
    cc_buffer_draw_string( self, x, y, "┏", &border_fg, &safe_bg );
    cc_buffer_draw_string( self, x + w - 1, y, "┓", &border_fg, &safe_bg );
    cc_buffer_draw_string( self, x, y + h - 1, "┗", &border_fg, &safe_bg );
    cc_buffer_draw_string( self, x + w - 1, y + h - 1, "┛", &border_fg, &safe_bg );

    // Horizontal Lines
    for( int i = x + 1; i < x + w - 1; ++i ){
        cc_buffer_draw_string( self, i, y, "━", &border_fg, &safe_bg );
        cc_buffer_draw_string( self, i, y + h - 1, "━", &border_fg, &safe_bg );
    }

    // Vertical Lines
    for( int j = y + 1; j < y + h - 1; ++j ){
        cc_buffer_draw_string( self, x, j, "┃", &border_fg, &safe_bg );
        cc_buffer_draw_string( self, x + w - 1, j, "┃", &border_fg, &safe_bg );
    }

    // Fill Center
    for( int j = y + 1; j < y + h - 1; ++j ){
        for( int i = x + 1; i < x + w - 1; ++i ){
            // 배경색 적용을 위해 공백 출력
            cc_buffer_draw_string( self, i, j, " ", fg, bg );
        }
    }
}

void cc_buffer_flush( cc_buffer_t* self )
{
    if( !self || !self->_front_buffer || !self->_back_buffer ) return;

    // 1. 출력 버퍼 할당 (Performance Optimization)
    // 화면 크기 * (Color seq + Move seq + Char bytes) + Margin
    // 매번 시스템 콜(printf)을 호출하는 오버헤드를 줄이기 위해 하나의 큰 문자열로 만듭니다.
    size_t capacity = (size_t)( self->_width * self->_height * 64 ) + 4096;
    char* out_buf = (char*)malloc( capacity );
    if( !out_buf ) return; // Fatal: Memory alloc failed

    char* ptr = out_buf;
    char* end = out_buf + capacity;

    // 최적화를 위한 상태 추적 변수
    cc_color_t last_fg = CC_COLOR_WHITE;
    cc_color_t last_bg = CC_COLOR_BLACK;
    bool color_set = false;

    // 터미널의 실제 커서 위치 추적 (1-based)
    // 초기값은 불가능한 좌표로 설정하여 첫 그리기 시 무조건 커서 이동 명령이 발생하게 함
    int term_cursor_y = -1;
    int term_cursor_x = -1;

    for( int y = 0; y < self->_height; ++y ){
        for( int x = 0; x < self->_width; ++x ){
            int idx = ( y * self->_width ) + x;
            cc_cell_t* back  = &self->_back_buffer[idx];
            cc_cell_t* front = &self->_front_buffer[idx];

            // A. 변경 감지 (Diff)
            // 이전 프레임(front)과 현재 프레임(back)이 같다면 그리기 건너뜀
            if( _is_cell_equal( back, front ) ){
                continue;
            }

            // B. Wide char Trail 스킵
            // (한글 등 2칸 문자 뒤에 오는 더미 데이터는 그리지 않고 상태만 동기화)
            if( back->_is_wide_trail ){
                *front = *back;
                continue;
            }

            // C. 커서 이동 최적화
            // 우리가 그리려는 좌표(0-based)를 터미널 좌표(1-based)로 변환
            int target_y = y + 1;
            int target_x = x + 1;

            // 터미널 커서가 이미 그릴 위치에 있다면 이동 명령 생략 (Sequential writing optimization)
            if( term_cursor_y != target_y || term_cursor_x != target_x ){
                // ANSI Move: \033[row;colH
                int written = snprintf( ptr, end - ptr, "\033[%d;%dH", target_y, target_x );
                if( written > 0 ) ptr += written;

                term_cursor_y = target_y;
                term_cursor_x = target_x;
            }

            // D. 색상 변경 최적화 (Stateful)
            // 이전 문자와 색상이 다를 때만 ANSI 색상 코드 전송
            if( !color_set || !cc_color_is_equal( &back->_fg, &last_fg ) ){
                char ansi[64];
                cc_color_to_ansi_fg( &back->_fg, ansi, sizeof(ansi) );
                int written = snprintf( ptr, end - ptr, "%s", ansi );
                if( written > 0 ) ptr += written;
                last_fg = back->_fg;
            }

            if( !color_set || !cc_color_is_equal( &back->_bg, &last_bg ) ){
                char ansi[64];
                cc_color_to_ansi_bg( &back->_bg, ansi, sizeof(ansi) );
                int written = snprintf( ptr, end - ptr, "%s", ansi );
                if( written > 0 ) ptr += written;
                last_bg = back->_bg;
            }
            color_set = true;

            // E. 문자 출력
            size_t ch_len = strlen( back->_ch );
            if( ptr + ch_len < end ){
                memcpy( ptr, back->_ch, ch_len );
                ptr += ch_len;
            }

            // F. Front 버퍼 동기화 (Commit)
            *front = *back;

            // G. 커서 위치 추적 업데이트
            // 문자 너비만큼 x 좌표 증가 (한글이면 +2, 영문이면 +1)
            size_t width_step = cc_util_get_string_width( back->_ch );
            term_cursor_x += (int)width_step;
        }
    }

    // 2. 최종 출력 (System Call)
    // 모아둔 버퍼를 한 번에 터미널로 전송
    if( ptr > out_buf ){
        // 색상 리셋을 마지막에 해주는 것이 안전함 (선택 사항)
        // const char* reset = "\033[0m";
        // if( ptr + 4 < end ) { memcpy(ptr, reset, 4); ptr += 4; }

        fwrite( out_buf, 1, ptr - out_buf, stdout );
        fflush( stdout );
    }

    free( out_buf );
}