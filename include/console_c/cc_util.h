#ifndef _CONSOLE_C_UTIL_H_
#define _CONSOLE_C_UTIL_H_

/** ------------------------------------------------------------------------------------
 * ConsoleC String Utility Module Header
 * ------------------------------------------------------------------------------------
 * UTF-8 문자열의 너비 계산, 문자열 분할, ANSI 코드 제거 등의 기능을 제공합니다.
 * std::string 및 std::vector 의존성을 제거하고 C 스타일로 포팅되었습니다.
 * ------------------------------------------------------------------------------------ */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 문자열 리스트 관리 구조체 (std::vector<std::string> 대체)
 * @details 사용 후 반드시 cc_util_string_list_free()를 호출하여 메모리를 해제해야 합니다.
 */
typedef struct
{
    char** _items; /**< 동적 할당된 문자열 배열 포인터 */
    size_t _count; /**< 리스트에 포함된 문자열 개수 */
} cc_string_list_t;

// -----------------------------------------------------------------------------
// Function Prototypes
// -----------------------------------------------------------------------------

/**
 * @brief UTF-8 문자열의 콘솔 출력 너비를 계산합니다.
 * @param str UTF-8 인코딩된 문자열
 * @return 콘솔상에서 차지하는 칸 수 (한글=2, 영문=1, ANSI코드=0)
 */
size_t cc_util_get_string_width( const char* str );

/**
 * @brief 문자열을 지정된 너비(max_width)에 맞춰 여러 줄로 분할합니다.
 * @param str 원본 문자열
 * @param max_width 한 줄당 최대 허용 너비
 * @param out_list [Output] 분할된 문자열 리스트가 저장될 구조체 포인터
 * @return 성공 시 true, 메모리 할당 실패 등의 경우 false
 * * @details
 * - 단어 중간에서 잘리지 않도록 멀티바이트 문자를 고려하여 자릅니다.
 * - out_list->_items는 내부적으로 malloc됩니다. 사용 후 cc_util_string_list_free 호출 필수.
 */
bool cc_util_split_string_by_width( const char* str, size_t max_width, cc_string_list_t* out_list );

/**
 * @brief cc_string_list_t 내부의 동적 할당된 메모리를 해제합니다.
 * @param list 해제할 리스트 객체 포인터
 */
void cc_util_string_list_free( cc_string_list_t* list );

/**
 * @brief 문자열에서 ANSI Escape Code(색상 등)를 제거한 순수 문자열을 반환합니다.
 * @param src 원본 문자열
 * @param out_buf [Output] 결과를 저장할 버퍼 (src의 길이 + 1 이상의 크기 권장)
 * @param buf_len out_buf의 크기
 * @return 성공 시 true, 버퍼 부족 등의 경우 false
 */
bool cc_util_strip_ansi_codes( const char* src, char* out_buf, size_t buf_len );

/**
 * @brief 해당 UTF-8 문자가 2칸(Double Width)을 차지하는지 확인합니다.
 * @param codepoint 유니코드 코드포인트
 */
bool cc_util_is_double_width( uint32_t codepoint );

#endif // _CONSOLE_C_UTIL_H_