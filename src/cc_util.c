/** ------------------------------------------------------------------------------------
 * ConsoleC String Utility Module Implementation
 * ------------------------------------------------------------------------------------
 * cc_util.h 의 구현부입니다.
 * ------------------------------------------------------------------------------------ */

#include "console_c/cc_util.h"

#include <stdlib.h> // malloc, free, realloc, strtoul
#include <string.h> // strlen, memcpy, strdup
#include <stdio.h>  // snprintf

// =========================================================================
// Internal Helper Functions (Static)
// =========================================================================

/**
 * @brief UTF-8 문자열에서 다음 문자의 바이트 길이와 코드포인트를 추출합니다.
 * @param str 분석할 문자열 포인터
 * @param out_codepoint [Output] 추출된 코드포인트
 * @return 해당 문자의 바이트 길이 (1~4), 유효하지 않으면 1 반환
 */
static int _get_utf8_char_info( const char* str, uint32_t* out_codepoint )
{
    unsigned char c = (unsigned char)str[0];
    uint32_t      val = 0;

    // 1 Byte (ASCII)
    if( c < 0x80 ){
        if( out_codepoint ) *out_codepoint = (uint32_t)c;
        return 1;
    }

    // 2 Bytes (0x80 ~ 0x7FF)
    if( ( c & 0xE0 ) == 0xC0 ){
        val = ( c & 0x1F ) << 6;
        val |= ( (unsigned char)str[1] & 0x3F );
        if( out_codepoint ) *out_codepoint = val;
        return 2;
    }

    // 3 Bytes (0x800 ~ 0xFFFF) - 한글 포함
    if( ( c & 0xF0 ) == 0xE0 ){
        val = ( c & 0x0F ) << 12;
        val |= ( (unsigned char)str[1] & 0x3F ) << 6;
        val |= ( (unsigned char)str[2] & 0x3F );
        if( out_codepoint ) *out_codepoint = val;
        return 3;
    }

    // 4 Bytes (0x10000 ~ 0x10FFFF) - 이모지 포함
    if( ( c & 0xF8 ) == 0xF0 ){
        val = ( c & 0x07 ) << 18;
        val |= ( (unsigned char)str[1] & 0x3F ) << 12;
        val |= ( (unsigned char)str[2] & 0x3F ) << 6;
        val |= ( (unsigned char)str[3] & 0x3F );
        if( out_codepoint ) *out_codepoint = val;
        return 4;
    }

    // Invalid UTF-8
    if( out_codepoint ) *out_codepoint = 0;
    return 1;
}

/**
 * @brief 너비를 차지하지 않는 특수 문자(Zero Width)인지 확인합니다.
 */
static bool _is_zero_width( uint32_t cp )
{
    if( cp == 0 ) return true; // Null

    // 1. ZWJ & ZWNJ
    if( cp == 0x200C || cp == 0x200D ) return true;

    // 2. Variation Selectors
    if( cp >= 0xFE00 && cp <= 0xFE0F ) return true;

    // 3. Combining Diacritical Marks
    if( cp >= 0x0300 && cp <= 0x036F ) return true;

    // 4. Skin Tone Modifiers
    if( cp >= 0x1F3FB && cp <= 0x1F3FF ) return true;

    // 5. Tags for Emoji
    if( cp >= 0xE0020 && cp <= 0xE007F ) return true;

    return false;
}

/**
 * @brief 리스트에 문자열을 추가하는 내부 헬퍼
 */
static bool _string_list_add( cc_string_list_t* list, const char* str, size_t len )
{
    // 리스트 확장
    size_t new_count = list->_count + 1;
    char** new_items = (char**)realloc( list->_items, sizeof(char*) * new_count );
    
    if( !new_items ){
        return false;
    }

    list->_items = new_items;

    // 문자열 복제
    char* str_copy = (char*)malloc( len + 1 );
    if( !str_copy ){
        return false;
    }
    memcpy( str_copy, str, len );
    str_copy[len] = '\0';

    list->_items[list->_count] = str_copy;
    list->_count = new_count;
    
    return true;
}

// =========================================================================
// Public API Implementation
// =========================================================================

bool cc_util_is_double_width( uint32_t cp )
{
    // 한글 범위 (Hangul Jamo, Syllables)
    if( ( cp >= 0x1100 && cp <= 0x11FF ) ||
        ( cp >= 0x3130 && cp <= 0x318F ) ||
        ( cp >= 0xAC00 && cp <= 0xD7A3 ) ){
        return true;
    }

    // CJK Unified Ideographs (한자)
    if( cp >= 0x4E00 && cp <= 0x9FFF ) return true;
    if( cp >= 0x3400 && cp <= 0x4DBF ) return true; // CJK Ext A
    if( cp >= 0xF900 && cp <= 0xFAFF ) return true; // CJK Compatibility

    // Fullwidth Forms (전각 문자)
    if( cp >= 0xFF01 && cp <= 0xFF60 ) return true;
    if( cp >= 0xFFE0 && cp <= 0xFFE6 ) return true;

    // Emoji & Symbols (Supplemental)
    if( cp >= 0x1F300 && cp <= 0x1F6FF ) return true;
    if( cp >= 0x1F900 && cp <= 0x1F9FF ) return true; // Supplemental Symbols
    if( cp >= 0x1F004 && cp <= 0x1F251 ) return true; // Enclosed CJK chars

    return false;
}

size_t cc_util_get_string_width( const char* str )
{
    if( !str ) return 0;

    size_t width = 0;
    size_t i     = 0;
    size_t len   = strlen( str );

    while( i < len ){
        // Check ANSI Escape Code
        if( str[i] == '\033' ){
            if( i + 1 < len && str[i+1] == '[' ){
                size_t j = i + 2;
                while( j < len ){
                    char c = str[j];
                    // ANSI 종료 문자 (m, K, H, J 등)
                    if( ( c >= 'A' && c <= 'Z' ) || ( c >= 'a' && c <= 'z' ) ){
                        j++;
                        break;
                    }
                    j++;
                }
                i = j; // Skip ANSI sequence
                continue;
            }
        }

        // Get Char Info
        uint32_t codepoint = 0;
        int byte_len = _get_utf8_char_info( &str[i], &codepoint );

        if( _is_zero_width( codepoint ) ){
            // 너비 추가 없음
        }
        else if( cc_util_is_double_width( codepoint ) ){
            width += 2;
        }
        else{
            width += 1;
        }

        i += byte_len;
    }

    return width;
}

bool cc_util_strip_ansi_codes( const char* src, char* out_buf, size_t buf_len )
{
    if( !src || !out_buf || buf_len == 0 ) return false;

    size_t i   = 0;
    size_t len = strlen( src );
    size_t idx = 0; // out_buf index

    while( i < len ){
        // Check ANSI
        if( src[i] == '\033' ){
            if( i + 1 < len && src[i+1] == '[' ){
                size_t j = i + 2;
                while( j < len ){
                    char c = src[j];
                    if( ( c >= 'A' && c <= 'Z' ) || ( c >= 'a' && c <= 'z' ) ){
                        j++;
                        break;
                    }
                    j++;
                }
                i = j;
                continue;
            }
        }

        // Copy char
        if( idx < buf_len - 1 ){
            out_buf[idx++] = src[i];
        }
        else{
            // Buffer overflow
            out_buf[idx] = '\0';
            return false;
        }
        i++;
    }

    out_buf[idx] = '\0';
    return true;
}

void cc_util_string_list_free( cc_string_list_t* list )
{
    if( !list ) return;

    if( list->_items ){
        for( size_t i = 0; i < list->_count; ++i ){
            if( list->_items[i] ){
                free( list->_items[i] );
            }
        }
        free( list->_items );
        list->_items = NULL;
    }
    list->_count = 0;
}

bool cc_util_split_string_by_width( const char* str, size_t max_width, cc_string_list_t* out_list )
{
    if( !str || !out_list ) return false;

    // 초기화
    out_list->_items = NULL;
    out_list->_count = 0;

    size_t i   = 0;
    size_t len = strlen( str );

    // 현재 라인 구성을 위한 동적 버퍼
    // 최악의 경우(ANSI가 엄청 많거나 전체가 한 줄)를 대비해 초기 크기를 넉넉히 잡거나 확장해야 함.
    // 여기서는 동적 확장을 간단히 구현.
    size_t line_buf_cap = 1024; 
    size_t line_buf_len = 0;
    char* line_buf     = (char*)malloc( line_buf_cap );
    if( !line_buf ) return false; // 메모리 부족

    size_t current_width = 0;

    while( i < len ){
        // size_t chunk_start = i;
        size_t chunk_len   = 0;
        size_t chunk_width = 0;

        // 1. ANSI Check
        if( str[i] == '\033' ){
            if( i + 1 < len && str[i+1] == '[' ){
                size_t j = i + 2;
                while( j < len ){
                    char c = str[j];
                    if( ( c >= 'A' && c <= 'Z' ) || ( c >= 'a' && c <= 'z' ) ){
                        j++;
                        break;
                    }
                    j++;
                }
                // ANSI는 너비 0
                chunk_len = j - i;
                chunk_width = 0;
            }
        }

        // 2. Character Check (ANSI가 아닐 경우)
        if( chunk_len == 0 ){
            uint32_t cp = 0;
            int bytes = _get_utf8_char_info( &str[i], &cp );
            chunk_len = bytes;
            chunk_width = cc_util_is_double_width( cp ) ? 2 : 1;
        }

        // 3. Wrap Check
        if( current_width + chunk_width > max_width ){
            // 현재 버퍼 내용을 리스트에 추가
            if( !_string_list_add( out_list, line_buf, line_buf_len ) ){
                free( line_buf );
                cc_util_string_list_free( out_list );
                return false;
            }

            // 라인 초기화
            line_buf_len = 0;
            current_width = 0;
        }

        // 4. Append to Buffer
        // 버퍼 확장 필요 여부 확인
        if( line_buf_len + chunk_len >= line_buf_cap ){
            size_t new_cap = line_buf_cap * 2 + chunk_len;
            char* new_buf = (char*)realloc( line_buf, new_cap );
            if( !new_buf ){
                free( line_buf );
                cc_util_string_list_free( out_list );
                return false;
            }
            line_buf = new_buf;
            line_buf_cap = new_cap;
        }

        memcpy( line_buf + line_buf_len, str + i, chunk_len );
        line_buf_len += chunk_len;
        current_width += chunk_width;

        i += chunk_len;
    }

    // 마지막 남은 라인 처리
    if( line_buf_len > 0 ){
        if( !_string_list_add( out_list, line_buf, line_buf_len ) ){
            free( line_buf );
            cc_util_string_list_free( out_list );
            return false;
        }
    }

    free( line_buf );
    return true;
}