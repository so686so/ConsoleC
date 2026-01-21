/** ------------------------------------------------------------------------------------
 * ConsoleC Example: Draw App (Paint)
 * ------------------------------------------------------------------------------------
 * 마우스와 키보드를 활용한 콘솔 그림판 애플리케이션입니다.
 * ------------------------------------------------------------------------------------ */

#define _POSIX_C_SOURCE 200809L // for strdup, etc.

#include "console_c.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <math.h>

// -----------------------------------------------------------------------------
// Constants & Types
// -----------------------------------------------------------------------------

static const char* DENSITY_CHARS = ".:+*oO#@";

typedef enum {
    APP_MODE_BRUSH,
    APP_MODE_ERASER,
    APP_MODE_COLOR_INPUT
} app_mode_e;

/**
 * @brief 캔버스 픽셀 구조체
 */
typedef struct {
    char       _ch[5];
    cc_color_t _fg;
    cc_color_t _bg;
} canvas_pixel_t;

/**
 * @brief UI 클릭 영역 (Hitbox)
 */
typedef struct draw_app_s draw_app_t; // Forward declaration
typedef void (*ui_action_f)( draw_app_t* app );

typedef struct {
    int         _x;
    int         _w;
    ui_action_f _action;
} ui_hitbox_t;

#define MAX_HITBOXES 32

/**
 * @brief 애플리케이션 상태 구조체
 */
struct draw_app_s {
    // State
    bool        _is_running;
    app_mode_e  _mode;
    
    // Canvas Data (1D Array for 2D map)
    canvas_pixel_t* _canvas_data;
    int             _canvas_w;
    int             _canvas_h;

    // Rendering Buffer
    cc_buffer_t* _screen_buffer;

    // Mouse State
    cc_coord_t  _mouse_cursor; // 0-based
    bool        _is_mouse_down;

    // Brush Tool
    int         _brush_density_idx;
    char        _brush_char[5];
    cc_color_t  _current_color;
    bool        _is_gradient_on;

    // Eraser Tool
    int         _eraser_size;

    // Input & UI
    char        _input_buf[32];
    char        _last_key_msg[64];

    // UI Hitboxes (Fixed array for simplicity)
    ui_hitbox_t _hitboxes[MAX_HITBOXES];
    int         _hitbox_count;
};

// -----------------------------------------------------------------------------
// Helper Functions
// -----------------------------------------------------------------------------

static int _clamp( int v, int min, int max ) {
    if( v < min ) return min;
    if( v > max ) return max;
    return v;
}

static void _get_time_string( char* buf, size_t size ) {
    time_t now = time( NULL );
    struct tm* t = localtime( &now );
    strftime( buf, size, "%H:%M:%S", t );
}

static void _update_brush_char( draw_app_t* app ) {
    size_t len = strlen( DENSITY_CHARS );
    if( app->_brush_density_idx >= (int)len ) app->_brush_density_idx = (int)len - 1;
    
    app->_brush_char[0] = DENSITY_CHARS[app->_brush_density_idx];
    app->_brush_char[1] = '\0';
}

static void _init_pixel( canvas_pixel_t* px ) {
    strcpy( px->_ch, " " );
    px->_fg = CC_COLOR_WHITE;
    px->_bg = CC_COLOR_BLACK;
}

// -----------------------------------------------------------------------------
// Canvas Management
// -----------------------------------------------------------------------------

static void _resize_canvas( draw_app_t* app, int w, int h ) {
    if( app->_canvas_w == w && app->_canvas_h == h ) return;

    // 새 캔버스 할당
    canvas_pixel_t* new_data = (canvas_pixel_t*)malloc( sizeof(canvas_pixel_t) * w * h );
    
    // 초기화
    for( int i = 0; i < w * h; ++i ){
        _init_pixel( &new_data[i] );
    }

    // 기존 데이터 복사
    if( app->_canvas_data ){
        int copy_w = ( w < app->_canvas_w ) ? w : app->_canvas_w;
        int copy_h = ( h < app->_canvas_h ) ? h : app->_canvas_h;

        for( int y = 0; y < copy_h; ++y ){
            for( int x = 0; x < copy_w; ++x ){
                int old_idx = y * app->_canvas_w + x;
                int new_idx = y * w + x;
                new_data[new_idx] = app->_canvas_data[old_idx];
            }
        }
        free( app->_canvas_data );
    }

    app->_canvas_data = new_data;
    app->_canvas_w = w;
    app->_canvas_h = h;
}

static void _clear_canvas( draw_app_t* app ) {
    if( !app->_canvas_data ) return;
    int count = app->_canvas_w * app->_canvas_h;
    for( int i = 0; i < count; ++i ){
        _init_pixel( &app->_canvas_data[i] );
    }
}

// -----------------------------------------------------------------------------
// Action Logic
// -----------------------------------------------------------------------------

static void _toggle_gradient( draw_app_t* app ) {
    app->_is_gradient_on = !app->_is_gradient_on;
    snprintf( app->_last_key_msg, sizeof(app->_last_key_msg), 
              app->_is_gradient_on ? "Gradient ON" : "Gradient OFF" );
}

static void _update_gradient( draw_app_t* app ) {
    if( !app->_is_gradient_on || app->_current_color._type != CC_COLOR_TYPE_RGB ) return;

    int delta = ( ( rand() % 3 ) - 1 ) * 3; // -3, 0, 3

    int r = _clamp( app->_current_color._rgb._r + delta, 0, 255 );
    int g = _clamp( app->_current_color._rgb._g + delta, 0, 255 );
    int b = _clamp( app->_current_color._rgb._b + delta, 0, 255 );

    cc_color_init_rgb( &app->_current_color, (uint8_t)r, (uint8_t)g, (uint8_t)b );
}

static void _action_draw( draw_app_t* app, int x, int y ) {
    if( !app->_canvas_data ) return;
    if( x < 0 || x >= app->_canvas_w || y < 0 || y >= app->_canvas_h ) return;

    // UI 영역 보호 (Top/Bottom Bar)
    cc_term_size_t size = cc_screen_get_size();
    if( y < 1 || y >= size._rows - 1 ) return;

    _update_gradient( app );

    int idx = y * app->_canvas_w + x;
    canvas_pixel_t* px = &app->_canvas_data[idx];
    
    strcpy( px->_ch, app->_brush_char );
    px->_fg = app->_current_color;
    px->_bg = CC_COLOR_BLACK;
}

static void _action_erase( draw_app_t* app, int center_x, int center_y ) {
    if( !app->_canvas_data ) return;

    int h = app->_eraser_size;
    int w = app->_eraser_size * 2;
    cc_term_size_t size = cc_screen_get_size();

    int start_y = center_y - ( h / 2 );
    int end_y   = start_y + h - 1;
    int start_x = center_x - ( w / 2 );
    int end_x   = start_x + w - 1;

    for( int y = start_y; y <= end_y; ++y ){
        for( int x = start_x; x <= end_x; ++x ){
            // Boundary Check
            if( y > 0 && y < size._rows - 1 && 
                y >= 0 && y < app->_canvas_h && 
                x >= 0 && x < app->_canvas_w ) 
            {
                int idx = y * app->_canvas_w + x;
                _init_pixel( &app->_canvas_data[idx] );
            }
        }
    }
}

static void _set_mode( draw_app_t* app, app_mode_e mode, const char* msg ) {
    app->_mode = mode;
    snprintf( app->_last_key_msg, sizeof(app->_last_key_msg), "%s", msg );
}

// -----------------------------------------------------------------------------
// Input Handlers
// -----------------------------------------------------------------------------

static void _handle_color_input( draw_app_t* app, const cc_input_event_t* event ) {
    if( event->_code == CC_KEY_ESC ) {
        _set_mode( app, APP_MODE_BRUSH, "Canceled" );
    }
    else if( event->_code == CC_KEY_ENTER ) {
        cc_color_t new_color;
        if( cc_color_init_hex( &new_color, app->_input_buf ) ) {
            app->_current_color = new_color;
            char msg[64];
            snprintf( msg, sizeof(msg), "Applied #%s", app->_input_buf );
            _set_mode( app, APP_MODE_BRUSH, msg );
        } else {
            snprintf( app->_last_key_msg, sizeof(app->_last_key_msg), "Invalid Hex!" );
        }
    }
    else if( event->_code == CC_KEY_BACKSPACE ) {
        size_t len = strlen( app->_input_buf );
        if( len > 0 ) app->_input_buf[len - 1] = '\0';
    }
    else {
        // Hex 문자 입력 (0~9, A~F)
        const char* key_str = cc_device_key_to_string( event->_code );
        if( strlen(key_str) == 1 && isxdigit(key_str[0]) ) {
            size_t len = strlen( app->_input_buf );
            if( len < 6 ) {
                app->_input_buf[len] = (char)toupper( key_str[0] );
                app->_input_buf[len + 1] = '\0';
            }
        }
    }
}

static void _handle_hotkeys( draw_app_t* app, const cc_input_event_t* event ) {
    const char* key_str = cc_device_key_to_string( event->_code );
    bool is_plus  = ( strcmp(key_str, "+") == 0 || strcmp(key_str, "=") == 0 || strcmp(key_str, "2") == 0 );
    bool is_minus = ( strcmp(key_str, "-") == 0 || strcmp(key_str, "_") == 0 || strcmp(key_str, "1") == 0 );

    if( app->_mode == APP_MODE_BRUSH ) {
        if( is_plus ) {
            app->_brush_density_idx++;
            _update_brush_char( app );
            snprintf( app->_last_key_msg, sizeof(app->_last_key_msg), "Density Up" );
        } else if( is_minus && app->_brush_density_idx > 0 ) {
            app->_brush_density_idx--;
            _update_brush_char( app );
            snprintf( app->_last_key_msg, sizeof(app->_last_key_msg), "Density Down" );
        }
    }
    else if( app->_mode == APP_MODE_ERASER ) {
        if( is_plus && app->_eraser_size < 10 ) {
            app->_eraser_size++;
            snprintf( app->_last_key_msg, sizeof(app->_last_key_msg), "Size Up" );
        } else if( is_minus && app->_eraser_size > 1 ) {
            app->_eraser_size--;
            snprintf( app->_last_key_msg, sizeof(app->_last_key_msg), "Size Down" );
        }
    }
}

static void _check_menu_click( draw_app_t* app, int rx, int ry ) {
    // Only Top Bar (ry == 0)
    if( ry != 0 ) return;

    for( int i = 0; i < app->_hitbox_count; ++i ) {
        ui_hitbox_t* hb = &app->_hitboxes[i];
        if( rx >= hb->_x && rx < hb->_x + hb->_w ) {
            if( hb->_action ) hb->_action( app );
            return;
        }
    }
}

static void _handle_mouse( draw_app_t* app, const cc_mouse_state_t* mouse ) {
    int mx = app->_mouse_cursor._x;
    int my = app->_mouse_cursor._y;

    if( mouse->_button == CC_MOUSE_BTN_LEFT ) {
        if( mouse->_action == CC_MOUSE_ACTION_PRESS ) app->_is_mouse_down = true;
        else if( mouse->_action == CC_MOUSE_ACTION_RELEASE ) app->_is_mouse_down = false;
    }

    // UI Click
    if( mouse->_button == CC_MOUSE_BTN_LEFT && mouse->_action == CC_MOUSE_ACTION_PRESS ) {
        if( my == 0 ) {
            _check_menu_click( app, mx, my );
            return;
        }
    }

    // Drawing Area
    cc_term_size_t size = cc_screen_get_size();
    if( my > 0 && my < size._rows - 1 ) {
        if( mouse->_button == CC_MOUSE_BTN_LEFT ) {
            if( mouse->_action == CC_MOUSE_ACTION_PRESS || mouse->_action == CC_MOUSE_ACTION_DRAG ) {
                if( app->_mode == APP_MODE_BRUSH ) _action_draw( app, mx, my );
                else if( app->_mode == APP_MODE_ERASER ) _action_erase( app, mx, my );
            }
        }
        else if( mouse->_button == CC_MOUSE_BTN_MIDDLE && mouse->_action == CC_MOUSE_ACTION_PRESS ) {
            _clear_canvas( app );
            snprintf( app->_last_key_msg, sizeof(app->_last_key_msg), "Canvas Cleared" );
        }
    }
}

static void _process_input( draw_app_t* app, const cc_input_event_t* event ) {
    // 0. Mouse Sync (1-based -> 0-based)
    if( event->_code == CC_KEY_MOUSE_EVENT ) {
        app->_mouse_cursor._x = event->_data._mouse._x - 1;
        app->_mouse_cursor._y = event->_data._mouse._y - 1;
    }

    // 1. Color Input Mode
    if( app->_mode == APP_MODE_COLOR_INPUT ) {
        _handle_color_input( app, event );
        return;
    }

    // 2. General Input
    switch( (int)event->_code ) {
        case CC_KEY_q: app->_is_running = false; break;
        
        case CC_KEY_F1: _set_mode( app, APP_MODE_BRUSH, "Mode: Brush" ); break;
        case CC_KEY_F2: _set_mode( app, APP_MODE_ERASER, "Mode: Eraser" ); break;
        case CC_KEY_F3: _toggle_gradient( app ); break;
        case CC_KEY_F4: 
            _set_mode( app, APP_MODE_COLOR_INPUT, "Input Hex..." ); 
            app->_input_buf[0] = '\0';
            break;
        case CC_KEY_RESIZE_EVENT:
            snprintf( app->_last_key_msg, sizeof(app->_last_key_msg), "Resized" );
            break;
        default: _handle_hotkeys( app, event ); break;
    }

    // 3. Mouse Handling
    if( event->_code == CC_KEY_MOUSE_EVENT ) {
        _handle_mouse( app, &event->_data._mouse );
    }
}

// -----------------------------------------------------------------------------
// Rendering
// -----------------------------------------------------------------------------

// UI Callback Wrappers
static void _cb_exit( draw_app_t* app ) { app->_is_running = false; }
static void _cb_brush( draw_app_t* app ) { _set_mode( app, APP_MODE_BRUSH, "Mode: Brush" ); }
static void _cb_eraser( draw_app_t* app ) { _set_mode( app, APP_MODE_ERASER, "Mode: Eraser" ); }
static void _cb_grad( draw_app_t* app ) { _toggle_gradient( app ); }
static void _cb_color( draw_app_t* app ) { _set_mode( app, APP_MODE_COLOR_INPUT, "Input..." ); app->_input_buf[0] = '\0'; }

static void _add_menu( draw_app_t* app, const char* label, bool active, ui_action_f act, int* current_x, const cc_color_t* bg ) {
    if( app->_hitbox_count >= MAX_HITBOXES ) return;

    char txt[64];
    snprintf( txt, sizeof(txt), " %s ", label );
    
    cc_color_t fg = active ? CC_COLOR_GREEN : CC_COLOR_WHITE;

    cc_buffer_draw_string( app->_screen_buffer, *current_x, 0, txt, &fg, bg );
    
    int len = (int)cc_util_get_string_width( txt );
    
    ui_hitbox_t* hb = &app->_hitboxes[app->_hitbox_count++];
    hb->_x = *current_x;
    hb->_w = len;
    hb->_action = act;

    *current_x += len;
    cc_buffer_draw_string( app->_screen_buffer, (*current_x)++, 0, "|", &CC_COLOR_WHITE, bg );
}

static void _draw_top_bar( draw_app_t* app ) {
    app->_hitbox_count = 0;
    
    cc_term_size_t size = cc_screen_get_size();
    cc_color_t bg; cc_color_init_rgb(&bg, 40, 40, 40);
    
    // BG Fill
    for( int x = 0; x < size._cols; ++x ) 
        cc_buffer_draw_string( app->_screen_buffer, x, 0, " ", &CC_COLOR_WHITE, &bg );

    int cx = 1;
    _add_menu( app, "[Q] Exit", false, _cb_exit, &cx, &bg );
    _add_menu( app, "[F1] Brush", app->_mode == APP_MODE_BRUSH, _cb_brush, &cx, &bg );
    _add_menu( app, "[F2] Eraser", app->_mode == APP_MODE_ERASER, _cb_eraser, &cx, &bg );
    
    char grad_txt[32];
    snprintf( grad_txt, sizeof(grad_txt), "[F3] Grad:%s", app->_is_gradient_on ? "ON " : "OFF" );
    _add_menu( app, grad_txt, app->_is_gradient_on, _cb_grad, &cx, &bg );
    
    _add_menu( app, "[F4] Color", app->_mode == APP_MODE_COLOR_INPUT, _cb_color, &cx, &bg );

    // Info
    char info_str[64] = {0};
    if( app->_mode == APP_MODE_BRUSH ) 
        snprintf( info_str, sizeof(info_str), " Dens :%d", app->_brush_density_idx + 1 );
    else if( app->_mode == APP_MODE_ERASER )
        snprintf( info_str, sizeof(info_str), " Size :%d", app->_eraser_size );
    
    cc_buffer_draw_string( app->_screen_buffer, cx, 0, info_str, &CC_COLOR_CYAN, &bg );
    cx += (int)cc_util_get_string_width( info_str );

    // Time
    char time_str[32];
    char time_buf[16];
    _get_time_string( time_buf, sizeof(time_buf) );
    snprintf( time_str, sizeof(time_str), " Time : %s", time_buf );
    
    int time_pos = size._cols - (int)cc_util_get_string_width( time_str ) - 1;
    if( time_pos > cx ) {
        cc_buffer_draw_string( app->_screen_buffer, time_pos, 0, time_str, &CC_COLOR_WHITE, &bg );
    }
}

static void _draw_bottom_bar( draw_app_t* app ) {
    cc_term_size_t size = cc_screen_get_size();
    int y = size._rows - 1;
    
    cc_color_t bg; cc_color_init_rgb(&bg, 40, 40, 40);
    
    // BG Fill
    for( int x = 0; x < size._cols; ++x ) 
        cc_buffer_draw_string( app->_screen_buffer, x, y, " ", &CC_COLOR_WHITE, &bg );
    
    if( app->_mode == APP_MODE_COLOR_INPUT ) {
        cc_color_t preview;
        bool valid = cc_color_init_hex( &preview, app->_input_buf );
        const cc_color_t* hash_fg = valid ? &CC_COLOR_WHITE : &CC_COLOR_RED;

        int cx = 1;
        cc_buffer_draw_string( app->_screen_buffer, cx, y, " Input: ", &CC_COLOR_WHITE, &bg ); cx += 8;
        cc_buffer_draw_string( app->_screen_buffer, cx, y, "#", hash_fg, &bg ); cx += 1;
        cc_buffer_draw_string( app->_screen_buffer, cx, y, app->_input_buf, &CC_COLOR_YELLOW, &bg ); 
        cx += (int)strlen( app->_input_buf );
        cc_buffer_draw_string( app->_screen_buffer, cx, y, "_", &CC_COLOR_WHITE, &bg ); cx += 2;

        if( valid ) {
            cc_buffer_draw_string( app->_screen_buffer, cx, y, "[Preview:  ]", &CC_COLOR_WHITE, &bg );
            cc_buffer_draw_string( app->_screen_buffer, cx + 9, y, "  ", &CC_COLOR_WHITE, &preview );
        }
    } else {
        char msg_buf[128];
        snprintf( msg_buf, sizeof(msg_buf), " %s", app->_last_key_msg );
        cc_buffer_draw_string( app->_screen_buffer, 1, y, msg_buf, &CC_COLOR_WHITE, &bg );

        // Current Color Block
        int col_x = 1 + (int)cc_util_get_string_width( msg_buf ) + 1;
        cc_buffer_draw_string( app->_screen_buffer, col_x, y, "  ", &CC_COLOR_WHITE, &app->_current_color );
    }

    // Mouse Pos
    char pos_str[32];
    snprintf( pos_str, sizeof(pos_str), "Pos(%d,%d)", app->_mouse_cursor._x, app->_mouse_cursor._y );
    int pos_x = size._cols - (int)cc_util_get_string_width( pos_str ) - 1;
    cc_buffer_draw_string( app->_screen_buffer, pos_x, y, pos_str, &CC_COLOR_WHITE, &bg );
}

static void _render( draw_app_t* app ) {
    cc_term_size_t size = cc_screen_get_size();

    // 1. Sync Sizes
    cc_buffer_resize( app->_screen_buffer, size._cols, size._rows );
    _resize_canvas( app, size._cols, size._rows );

    // 2. Clear Buffer
    cc_buffer_clear( app->_screen_buffer, &CC_COLOR_BLACK );

    // 3. Draw Canvas
    int draw_h = ( app->_canvas_h < size._rows ) ? app->_canvas_h : size._rows;
    int draw_w = ( app->_canvas_w < size._cols ) ? app->_canvas_w : size._cols;

    for( int y = 0; y < draw_h; ++y ){
        for( int x = 0; x < draw_w; ++x ){
            int idx = y * app->_canvas_w + x;
            canvas_pixel_t* px = &app->_canvas_data[idx];
            
            if( strcmp( px->_ch, " " ) != 0 ) {
                cc_buffer_draw_string( app->_screen_buffer, x, y, px->_ch, &px->_fg, &px->_bg );
            }
        }
    }

    // 4. Eraser Overlay
    if( app->_mode == APP_MODE_ERASER && app->_is_mouse_down ) {
        if( app->_mouse_cursor._y > 0 && app->_mouse_cursor._y < size._rows - 1 ) {
            int h = app->_eraser_size;
            int w = app->_eraser_size * 2;
            int start_y = app->_mouse_cursor._y - ( h / 2 );
            int start_x = app->_mouse_cursor._x - ( w / 2 );

            cc_color_t gray; cc_color_init_rgb( &gray, 128, 128, 128 );
            cc_buffer_draw_box( app->_screen_buffer, start_x, start_y, w, h, &CC_COLOR_BLACK, &gray, false );
        }
    }

    // 5. UI
    _draw_top_bar( app );
    _draw_bottom_bar( app );

    // 6. Flush
    cc_buffer_flush( app->_screen_buffer );
}

// -----------------------------------------------------------------------------
// App Lifecycle
// -----------------------------------------------------------------------------

int main( void )
{
    srand( (unsigned int)time(NULL) );

    // App Init
    draw_app_t app;
    memset( &app, 0, sizeof(draw_app_t) );

    app._is_running = true;
    app._mode = APP_MODE_BRUSH;
    app._brush_density_idx = 3;
    app._eraser_size = 3;
    app._current_color = CC_COLOR_WHITE;
    strcpy( app._last_key_msg, "Ready" );
    
    _update_brush_char( &app );

    // Console Init
    cc_device_init();
    cc_device_enable_mouse( true );
    cc_screen_set_back_color( &CC_COLOR_BLACK );
    cc_screen_clear();

    app._screen_buffer = cc_buffer_create( 80, 24 );

    // First Render
    _render( &app );

    // Main Loop
    while( app._is_running )
    {
        // 10ms wait
        cc_key_code_e key = cc_device_get_input( 10 );
        if( key != CC_KEY_NONE ) {
            cc_input_event_t evt;
            cc_device_inspect( key, &evt );
            _process_input( &app, &evt );
        }

        _render( &app );
    }

    // Cleanup
    cc_device_enable_mouse( false );
    cc_screen_reset_color();
    cc_screen_clear();
    
    cc_buffer_destroy( app._screen_buffer );
    if( app._canvas_data ) free( app._canvas_data );
    
    cc_device_deinit();
    printf( "DrawApp Terminated.\n" );

    return 0;
}