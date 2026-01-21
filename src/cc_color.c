/** ------------------------------------------------------------------------------------
 * ConsoleC Color Module Implementation
 * ------------------------------------------------------------------------------------
 * cc_color.h 의 구현부입니다.
 * ------------------------------------------------------------------------------------ */

#include "console_c/cc_color.h"

#include <stdio.h>  // snprintf (문자열 포맷팅)
#include <string.h> // strlen, strncmp (문자열 길이 및 비교)
#include <stdlib.h> // strtoul (Hex 문자열 파싱)
#include <ctype.h>  // isxdigit (Hex 문자 판별)

// -----------------------------------------------------------------------------
// Global Presets Definition
// -----------------------------------------------------------------------------
const cc_color_t CC_COLOR_BLACK   = { CC_COLOR_TYPE_RGB,   { 0,   0,   0   } };
const cc_color_t CC_COLOR_WHITE   = { CC_COLOR_TYPE_RGB,   { 255, 255, 255 } };
const cc_color_t CC_COLOR_RED     = { CC_COLOR_TYPE_RGB,   { 255, 0,   0   } };
const cc_color_t CC_COLOR_GREEN   = { CC_COLOR_TYPE_RGB,   { 0,   255, 0   } };
const cc_color_t CC_COLOR_BLUE    = { CC_COLOR_TYPE_RGB,   { 0,   0,   255 } };
const cc_color_t CC_COLOR_YELLOW  = { CC_COLOR_TYPE_RGB,   { 255, 255, 0   } };
const cc_color_t CC_COLOR_CYAN    = { CC_COLOR_TYPE_RGB,   { 0,   255, 255 } };
const cc_color_t CC_COLOR_MAGENTA = { CC_COLOR_TYPE_RGB,   { 255, 0,   255 } };
const cc_color_t CC_COLOR_GRAY    = { CC_COLOR_TYPE_RGB,   { 128, 128, 128 } };
const cc_color_t CC_COLOR_RESET   = { CC_COLOR_TYPE_RESET, { 0,   0,   0   } };

// -----------------------------------------------------------------------------
// Implementation
// -----------------------------------------------------------------------------

void cc_color_init_rgb( cc_color_t* out_color, uint8_t r, uint8_t g, uint8_t b )
{
    if( !out_color ){
        return;
    }

    out_color->_type   = CC_COLOR_TYPE_RGB;
    out_color->_rgb._r = r;
    out_color->_rgb._g = g;
    out_color->_rgb._b = b;
}

bool cc_color_init_hex( cc_color_t* out_color, const char* hex_code )
{
    if( !out_color ){
        return false;
    }

    // 기본적으로 NONE으로 설정 (실패 대비)
    out_color->_type = CC_COLOR_TYPE_NONE;

    if( !hex_code ){
        return false;
    }

    // '#' 접두사 처리
    const char* ptr = hex_code;
    if( ptr[0] == '#' ){
        ptr++;
    }

    // 길이 검증 (반드시 6글자여야 함: RRGGBB)
    if( strlen( ptr ) != 6 ){
        return false;
    }

    // 16진수 문자 유효성 검증
    for( int i = 0; i < 6; ++i ){
        if( !isxdigit( ptr[i] ) ){
            return false;
        }
    }

    // 파싱 (strtoul은 0x 접두사 없이도 16진수 처리 가능하도록 base 16 설정)
    char* end_ptr   = NULL;
    uint32_t color_val = (uint32_t)strtoul( ptr, &end_ptr, 16 );

    // 파싱 실패 혹은 끝까지 읽지 못한 경우
    if( ptr == end_ptr ){
        return false;
    }

    // 값 할당
    out_color->_type   = CC_COLOR_TYPE_RGB;
    out_color->_rgb._r = (uint8_t)( ( color_val >> 16 ) & 0xFF );
    out_color->_rgb._g = (uint8_t)( ( color_val >> 8 ) & 0xFF );
    out_color->_rgb._b = (uint8_t)( color_val & 0xFF );

    return true;
}

void cc_color_init_type( cc_color_t* out_color, cc_color_type_e type )
{
    if( !out_color ){
        return;
    }

    out_color->_type   = type;
    out_color->_rgb._r = 0;
    out_color->_rgb._g = 0;
    out_color->_rgb._b = 0;
}

const char* cc_color_to_ansi_fg( const cc_color_t* self, char* buf, size_t buf_len )
{
    if( !self || !buf || buf_len == 0 ){
        return NULL;
    }

    if( self->_type == CC_COLOR_TYPE_RESET ){
        // Reset Code
        snprintf( buf, buf_len, "\033[0m" );
    }
    else if( self->_type == CC_COLOR_TYPE_RGB ){
        // Foreground RGB: \033[38;2;R;G;Bm
        snprintf( buf, buf_len, "\033[38;2;%d;%d;%dm",
                  self->_rgb._r, self->_rgb._g, self->_rgb._b );
    }
    else{
        // NONE or Invalid
        return NULL;
    }

    return buf;
}

const char* cc_color_to_ansi_bg( const cc_color_t* self, char* buf, size_t buf_len )
{
    if( !self || !buf || buf_len == 0 ){
        return NULL;
    }

    if( self->_type == CC_COLOR_TYPE_RESET ){
        // Reset (배경색 리셋은 보통 0m으로 전체 리셋하거나 49m을 쓰지만 여기선 0m 통일)
        snprintf( buf, buf_len, "\033[0m" );
    }
    else if( self->_type == CC_COLOR_TYPE_RGB ){
        // Background RGB: \033[48;2;R;G;Bm
        snprintf( buf, buf_len, "\033[48;2;%d;%d;%dm",
                  self->_rgb._r, self->_rgb._g, self->_rgb._b );
    }
    else{
        return NULL;
    }

    return buf;
}

const char* cc_color_to_hex( const cc_color_t* self, char* buf, size_t buf_len )
{
    if( !self || !buf || buf_len == 0 ){
        return NULL;
    }

    if( self->_type == CC_COLOR_TYPE_RGB ){
        // #RRGGBB (총 7바이트 + NULL)
        if( buf_len < 8 ){
            return NULL; // 버퍼 부족
        }
        snprintf( buf, buf_len, "#%02X%02X%02X",
                  self->_rgb._r, self->_rgb._g, self->_rgb._b );
    }
    else{
        return NULL;
    }

    return buf;
}

bool cc_color_is_equal( const cc_color_t* lhs, const cc_color_t* rhs )
{
    if( !lhs || !rhs ){
        return false;
    }

    if( lhs->_type != rhs->_type ){
        return false;
    }

    if( lhs->_type == CC_COLOR_TYPE_RGB ){
        return ( lhs->_rgb._r == rhs->_rgb._r ) &&
               ( lhs->_rgb._g == rhs->_rgb._g ) &&
               ( lhs->_rgb._b == rhs->_rgb._b );
    }

    // RESET이나 NONE은 타입만 같으면 같은 것으로 간주
    return true;
}

bool cc_color_is_valid( const cc_color_t* self )
{
    if( !self ){
        return false;
    }
    return self->_type != CC_COLOR_TYPE_NONE;
}

bool cc_color_is_rgb( const cc_color_t* self )
{
    if( !self ){
        return false;
    }
    return self->_type == CC_COLOR_TYPE_RGB;
}