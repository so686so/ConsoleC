/** ------------------------------------------------------------------------------------
 * ConsoleC Example: Buffer Test
 * ------------------------------------------------------------------------------------
 * 더블 버퍼링, 컬러 애니메이션, 입력 처리를 테스트하는 예제입니다.
 * ------------------------------------------------------------------------------------ */

#include "console_c.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // usleep

int main( void )
{
    // 1. 초기 설정 (Init)
    cc_device_init();
    cc_device_enable_mouse( false );
    cc_screen_clear();

    // 초기 버퍼 생성 (크기는 루프 안에서 Resize로 맞춰짐)
    cc_buffer_t* buffer = cc_buffer_create( 80, 24 );
    if( !buffer ){
        fprintf( stderr, "Failed to create buffer.\n" );
        cc_device_deinit();
        return -1;
    }

    int x  = 2, y  = 2;
    int dx = 1, dy = 1;

    long long frame_count = 0;
    bool is_running = true;

    // 자주 쓰는 색상 객체 미리 준비 (최적화)
    const cc_color_t* c_black  = &CC_COLOR_BLACK;
    const cc_color_t* c_white  = &CC_COLOR_WHITE;
    const cc_color_t* c_yellow = &CC_COLOR_YELLOW;
    const cc_color_t* c_blue   = &CC_COLOR_BLUE;
    
    // 배경 패턴용 회색
    cc_color_t c_gray;
    cc_color_init_rgb( &c_gray, 200, 200, 200 );

    // 박스 배경 (어두운 회색)
    cc_color_t c_dark;
    cc_color_init_rgb( &c_dark, 20, 20, 20 );

    while( is_running )
    {
        // --- [입력 처리] ---
        // 1ms 대기 (Non-blocking에 가까움)
        cc_key_code_e key = cc_device_get_input( 1 );
        
        if( key != CC_KEY_NONE ){
            // Q 혹은 ESC 종료
            if( key == CC_KEY_ESC || key == (cc_key_code_e)'q' ){
                is_running = false;
            }
        }

        // --- [상태 업데이트] ---
        cc_term_size_t size = cc_screen_get_size();

        // 1. 버퍼 리사이즈 & 초기화
        cc_buffer_resize( buffer, size._cols, size._rows );
        cc_buffer_clear( buffer, c_black );

        // 2. 배경 패턴 그리기 (플리커링 테스트용)
        for( int r = 0; r < size._rows; r += 2 ){
            for( int c = 0; c < size._cols; c += 4 ){
                cc_buffer_draw_string( buffer, c, r, ".", &c_gray, c_black );
            }
        }

        // 3. 움직이는 박스 좌표 계산
        x += dx;
        y += dy;

        // 경계 체크
        if( x <= 1 || x >= size._cols - 24 ) dx = -dx;
        if( y <= 1 || y >= size._rows - 12 ) dy = -dy;

        // 4. 박스 그리기 (색상 애니메이션)
        uint8_t r_val = (uint8_t)( ( frame_count * 2 ) % 255 );
        uint8_t g_val = (uint8_t)( ( frame_count * 3 ) % 255 );
        uint8_t b_val = (uint8_t)( ( frame_count * 5 ) % 255 );
        
        cc_color_t box_color;
        cc_color_init_rgb( &box_color, r_val, g_val, b_val );

        cc_buffer_draw_box( buffer, x, y, 24, 12, &box_color, &c_dark, false );
        cc_buffer_draw_string( buffer, x + 8, y + 5, "NO FLICKER", c_white, c_black );

        // 5. 프레임 카운터 및 안내
        char info_buf[128];
        snprintf( info_buf, sizeof(info_buf), " Frame: %lld | Press [Q] to Quit ", frame_count );
        cc_buffer_draw_string( buffer, 2, 0, info_buf, c_yellow, c_blue );

        // --- [렌더링] ---
        cc_buffer_flush( buffer );

        frame_count++;

        // 약 60 FPS (16ms)
        usleep( 16000 );
    }

    // 정리 (Cleanup)
    cc_buffer_destroy( buffer );
    
    cc_screen_clear();
    cc_screen_reset_color();
    printf( "Test Finished.\n" );

    cc_device_deinit();

    return 0;
}