/** ------------------------------------------------------------------------------------
 * ConsoleC Example: Inventory App
 * ------------------------------------------------------------------------------------
 * 윈도우 이동/리사이즈, 아이템 드래그 앤 드롭, 레이아웃 관리 기능을 테스트합니다.
 * ------------------------------------------------------------------------------------ */

#define _POSIX_C_SOURCE 200809L

#include "console_c.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h> // usleep

// -----------------------------------------------------------------------------
// Constants & Types
// -----------------------------------------------------------------------------

#define MAX_TITLE_LEN 64
#define MAX_TEXT_LEN  128

typedef struct {
    int x, y, w, h;
} rect_t;

typedef struct {
    char name[MAX_TEXT_LEN];
    char desc[MAX_TEXT_LEN * 2];
} item_t;

/**
 * @brief 동적 아이템 리스트 (std::vector<Item> 대체)
 */
typedef struct {
    item_t* _data;
    int     _count;
    int     _capacity;
} item_list_t;

typedef struct {
    char        _title[MAX_TITLE_LEN];
    rect_t      _rect;
    rect_t      _saved_rect;
    item_list_t _items;
    
    // UI States
    bool        _is_red_border;
    bool        _is_green_border;
} inventory_t;

typedef enum {
    DRAG_NONE,
    DRAG_WINDOW_MOVE,
    DRAG_WINDOW_RESIZE,
    DRAG_ITEM_MOVE
} drag_mode_e;

typedef enum {
    VIEW_NORMAL,
    VIEW_MAXIMIZED
} view_mode_e;

typedef struct {
    // State
    bool            _is_running;
    bool            _need_render;
    view_mode_e     _view_mode;
    
    // Data
    inventory_t* _inventories;
    int             _inv_count;
    
    // Resources
    cc_buffer_t* _screen_buffer;

    // Drag State
    drag_mode_e     _drag_mode;
    int             _drag_target_idx; // Window Index
    int             _drag_item_idx;   // Item Index inside Window
    cc_coord_t      _drag_offset;     // For window move
    cc_coord_t      _drag_start_win_pos; 
    item_t          _dragging_item;

    // Mouse
    cc_coord_t      _mouse_cursor; // 0-based

    // UI
    char            _log_msg[256];
    
} app_state_t;

// -----------------------------------------------------------------------------
// Rect Utilities
// -----------------------------------------------------------------------------

static bool _rect_contains( rect_t r, int px, int py ) {
    return ( px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h );
}

static bool _rect_intersects( rect_t a, rect_t b ) {
    return ( a.x < b.x + b.w && a.x + a.w > b.x &&
             a.y < b.y + b.h && a.y + a.h > b.y );
}

// -----------------------------------------------------------------------------
// Item List Management (Vector replacement)
// -----------------------------------------------------------------------------

static void _list_init( item_list_t* list ) {
    list->_data = NULL;
    list->_count = 0;
    list->_capacity = 0;
}

static void _list_free( item_list_t* list ) {
    if( list->_data ) free( list->_data );
    _list_init( list );
}

static void _list_add( item_list_t* list, item_t item ) {
    if( list->_count >= list->_capacity ) {
        int new_cap = ( list->_capacity == 0 ) ? 4 : list->_capacity * 2;
        item_t* new_data = (item_t*)realloc( list->_data, sizeof(item_t) * new_cap );
        if( !new_data ) return; // Fatal
        list->_data = new_data;
        list->_capacity = new_cap;
    }
    list->_data[list->_count++] = item;
}

static void _list_remove_at( item_list_t* list, int index ) {
    if( index < 0 || index >= list->_count ) return;
    
    // Shift elements
    if( index < list->_count - 1 ) {
        memmove( &list->_data[index], &list->_data[index + 1], 
                 sizeof(item_t) * ( list->_count - index - 1 ) );
    }
    list->_count--;
}

static int _compare_items( const void* a, const void* b ) {
    const item_t* ia = (const item_t*)a;
    const item_t* ib = (const item_t*)b;
    return strcmp( ia->name, ib->name );
}

static void _list_sort( item_list_t* list ) {
    if( list->_count > 1 ) {
        qsort( list->_data, list->_count, sizeof(item_t), _compare_items );
    }
}

// -----------------------------------------------------------------------------
// Inventory Logic
// -----------------------------------------------------------------------------

static void _inv_init( inventory_t* inv, const char* title, int x, int y, int w ) {
    strncpy( inv->_title, title, MAX_TITLE_LEN - 1 );
    inv->_rect = (rect_t){ x, y, w, 0 };
    inv->_saved_rect = inv->_rect;
    _list_init( &inv->_items );
    inv->_is_red_border = false;
    inv->_is_green_border = false;
}

static int _inv_get_calc_height( const inventory_t* inv ) {
    // Border(2) + Title(1) + Items(count) + Bottom(1) -> Simple: 3 + count
    // C++ logic: 3 + items.size() + 1 ?? -> Code says: return 3 + (int)items.size() + 1;
    // Let's follow C++ logic strictly: 3 (Top/Title/Sep) + count + 1 (Bottom)
    return 4 + inv->_items._count;
}

static void _inv_update_height( inventory_t* inv ) {
    inv->_rect.h = _inv_get_calc_height( inv );
}

// UTF-8 Aware Truncate
static void _truncate_text( const char* src, int max_width, char* out_buf, size_t buf_len ) {
    size_t w = cc_util_get_string_width( src );
    if( (int)w <= max_width ) {
        snprintf( out_buf, buf_len, "%s", src );
        return;
    }

    // Copy src to temp modifiable buffer
    char temp[MAX_TEXT_LEN * 2];
    snprintf( temp, sizeof(temp), "%s", src );
    size_t len = strlen( temp );

    const char* dot = "..";
    
    // While (width(temp + "..") > max) -> pop back
    while( len > 0 ) {
        // Append dots check
        char check_buf[MAX_TEXT_LEN * 3];
        snprintf( check_buf, sizeof(check_buf), "%s%s", temp, dot );
        if( (int)cc_util_get_string_width( check_buf ) <= max_width ) {
            snprintf( out_buf, buf_len, "%s", check_buf );
            return;
        }

        // Pop last char (UTF-8 aware)
        // Find start of last char
        char* last = temp + len; // null terminator
        do {
            last--;
            len--;
        } while( len > 0 && ( (*last & 0xC0) == 0x80 ) ); // Skip continuation bytes
        *last = '\0';
    }
    
    snprintf( out_buf, buf_len, "%s", ".." );
}

static void _inv_draw( inventory_t* inv, cc_buffer_t* buf ) {
    cc_color_t fg = CC_COLOR_WHITE;
    if( inv->_is_red_border ) fg = CC_COLOR_RED;
    else if( inv->_is_green_border ) fg = CC_COLOR_GREEN;
    
    cc_color_t bg = CC_COLOR_BLACK;
    cc_color_t yellow = CC_COLOR_YELLOW;

    // Draw Box
    cc_buffer_draw_box( buf, inv->_rect.x, inv->_rect.y, inv->_rect.w, inv->_rect.h, &fg, &bg, inv->_is_red_border );
    
    // Handle Icon "[-] "
    cc_buffer_draw_string( buf, inv->_rect.x + 1, inv->_rect.y, "[-] ", &fg, &bg );

    // Separator
    cc_buffer_draw_string( buf, inv->_rect.x, inv->_rect.y + 2, "┣", &fg, &bg );
    cc_buffer_draw_string( buf, inv->_rect.x + inv->_rect.w - 1, inv->_rect.y + 2, "┫", &fg, &bg );
    for( int i = inv->_rect.x + 1; i < inv->_rect.x + inv->_rect.w - 1; ++i ) {
        cc_buffer_draw_string( buf, i, inv->_rect.y + 2, "━", &fg, &bg );
    }

    // Title
    int content_w = inv->_rect.w - 2;
    char display_title[MAX_TITLE_LEN];
    _truncate_text( inv->_title, content_w, display_title, sizeof(display_title) );
    
    int title_w = (int)cc_util_get_string_width( display_title );
    int center_x = inv->_rect.x + ( inv->_rect.w - title_w ) / 2;
    cc_buffer_draw_string( buf, center_x, inv->_rect.y + 1, display_title, &yellow, &bg );

    // Items
    for( int i = 0; i < inv->_items._count; ++i ) {
        int row_y = inv->_rect.y + 3 + i;
        if( row_y >= inv->_rect.y + inv->_rect.h - 1 ) break;

        char prefix[16];
        snprintf( prefix, sizeof(prefix), "%d. ", i + 1 );
        int prefix_w = (int)cc_util_get_string_width( prefix );
        
        int item_space = content_w - prefix_w - 1;
        char item_name[MAX_TEXT_LEN];
        _truncate_text( inv->_items._data[i].name, item_space, item_name, sizeof(item_name) );

        char line[MAX_TEXT_LEN * 2];
        snprintf( line, sizeof(line), "%s%s", prefix, item_name );
        
        cc_buffer_draw_string( buf, inv->_rect.x + 2, row_y, line, &CC_COLOR_WHITE, &bg );
    }
}

static bool _inv_hit_handle( inventory_t* inv, int px, int py ) {
    return ( py == inv->_rect.y && px >= inv->_rect.x && px <= inv->_rect.x + 4 );
}

static bool _inv_hit_resize( inventory_t* inv, int px, int py ) {
    bool right = ( px == inv->_rect.x + inv->_rect.w - 1 && py >= inv->_rect.y && py < inv->_rect.y + inv->_rect.h );
    bool bottom = ( py == inv->_rect.y + inv->_rect.h - 1 && px >= inv->_rect.x && px < inv->_rect.x + inv->_rect.w );
    return right || bottom;
}

static int _inv_hit_item_index( inventory_t* inv, int px, int py ) {
    if( px > inv->_rect.x && px < inv->_rect.x + inv->_rect.w - 1 ) {
        int row = py - ( inv->_rect.y + 3 );
        if( row >= 0 && row < inv->_items._count ) return row;
    }
    return -1;
}

// -----------------------------------------------------------------------------
// App Logic
// -----------------------------------------------------------------------------

static void _app_save_layout( app_state_t* app ) {
    for( int i = 0; i < app->_inv_count; ++i ) {
        app->_inventories[i]._saved_rect = app->_inventories[i]._rect;
    }
}

static void _app_restore_layout( app_state_t* app ) {
    for( int i = 0; i < app->_inv_count; ++i ) {
        inventory_t* inv = &app->_inventories[i];
        inv->_rect.x = inv->_saved_rect.x;
        inv->_rect.y = inv->_saved_rect.y;
        inv->_rect.w = inv->_saved_rect.w;
        // h is recalculated in render loop
    }
}

// For qsort
typedef struct { int score; int idx; } sort_helper_t;
static int _compare_layout( const void* a, const void* b ) {
    return ((sort_helper_t*)a)->score - ((sort_helper_t*)b)->score;
}

static void _app_apply_maximized( app_state_t* app ) {
    cc_term_size_t size = cc_screen_get_size();
    int part_w = size._cols / 5;

    sort_helper_t sorted[5];
    for( int i = 0; i < app->_inv_count; ++i ) {
        sorted[i].idx = i;
        sorted[i].score = app->_inventories[i]._rect.x * 1000 + app->_inventories[i]._rect.y;
    }
    qsort( sorted, app->_inv_count, sizeof(sort_helper_t), _compare_layout );

    for( int k = 0; k < app->_inv_count; ++k ) {
        int idx = sorted[k].idx;
        inventory_t* inv = &app->_inventories[idx];
        
        int x_pos = 1 + ( k * part_w );
        inv->_rect.x = x_pos;
        inv->_rect.y = 2;
        inv->_rect.w = part_w - 1;
        inv->_rect.h = size._rows - 3; // Fixed height in maximized mode
    }
}

static bool _app_check_collision( app_state_t* app, int target_idx, rect_t test_rect ) {
    for( int i = 0; i < app->_inv_count; ++i ) {
        if( i == target_idx ) continue;
        if( _rect_intersects( test_rect, app->_inventories[i]._rect ) ) return true;
    }
    return false;
}

static cc_coord_t _app_find_valid_pos( app_state_t* app, int target_idx ) {
    inventory_t* target = &app->_inventories[target_idx];
    int w = target->_rect.w;
    int h = _inv_get_calc_height( target );
    cc_term_size_t size = cc_screen_get_size();

    for( int y = 2; y < size._rows - h; y += 2 ) {
        for( int x = 1; x < size._cols - w; x += 2 ) {
            rect_t cand = { x, y, w, h };
            bool coll = false;
            for( int i = 0; i < app->_inv_count; ++i ) {
                if( i == target_idx ) continue;
                if( _rect_intersects( cand, app->_inventories[i]._rect ) ) { 
                    coll = true; break; 
                }
            }
            if( !coll ) return (cc_coord_t){ x, y };
        }
    }
    return (cc_coord_t){ 1, 2 };
}

static void _app_check_window_collision( app_state_t* app, int current_idx ) {
    inventory_t* curr = &app->_inventories[current_idx];
    curr->_is_red_border = false;

    // Normal view: update height unless resizing
    if( app->_view_mode == VIEW_NORMAL && 
       !( app->_drag_mode == DRAG_WINDOW_RESIZE && app->_drag_target_idx == current_idx ) ) {
        _inv_update_height( curr );
    }

    for( int i = 0; i < app->_inv_count; ++i ) {
        if( i == current_idx ) continue;
        if( _rect_intersects( curr->_rect, app->_inventories[i]._rect ) ) {
            curr->_is_red_border = true;
            break;
        }
    }
}

// -----------------------------------------------------------------------------
// Input Handling
// -----------------------------------------------------------------------------

static void _handle_mouse_press( app_state_t* app, int mx, int my ) {
    // Top Bar Click Check
    if( my == 0 ) {
        // Simple hit test for fixed menus
        // [Q]uit
        int cx = 1; 
        if( mx >= cx && mx < cx+6 ) { app->_is_running = false; return; }
        cx += 7; // "| "
        // [F1]
        if( mx >= cx && mx < cx+8 ) { 
            if( app->_view_mode == VIEW_NORMAL ) {
                _app_save_layout( app ); _app_apply_maximized( app ); 
                app->_view_mode = VIEW_MAXIMIZED; strcpy(app->_log_msg, "Mode: Maximized");
            }
            return;
        }
        cx += 9;
        // [F2]
        if( mx >= cx && mx < cx+12 ) {
            if( app->_view_mode == VIEW_MAXIMIZED ) {
                _app_restore_layout( app ); app->_view_mode = VIEW_NORMAL;
                strcpy(app->_log_msg, "Mode: Normal");
            }
            return;
        }
        return;
    }

    // Windows
    for( int i = app->_inv_count - 1; i >= 0; --i ) {
        inventory_t* inv = &app->_inventories[i];
        if( app->_view_mode == VIEW_NORMAL ) _inv_update_height( inv );

        bool allow_win_ops = ( app->_view_mode == VIEW_NORMAL );

        if( allow_win_ops && _inv_hit_handle( inv, mx, my ) ) {
            app->_drag_mode = DRAG_WINDOW_MOVE;
            app->_drag_target_idx = i;
            app->_drag_offset = (cc_coord_t){ mx - inv->_rect.x, my - inv->_rect.y };
            app->_drag_start_win_pos = (cc_coord_t){ inv->_rect.x, inv->_rect.y };
            return;
        }
        
        if( allow_win_ops && _inv_hit_resize( inv, mx, my ) ) {
            app->_drag_mode = DRAG_WINDOW_RESIZE;
            app->_drag_target_idx = i;
            return;
        }

        int item_idx = _inv_hit_item_index( inv, mx, my );
        if( item_idx != -1 ) {
            snprintf( app->_log_msg, sizeof(app->_log_msg), "Selected: %s", inv->_items._data[item_idx].name );
            app->_drag_mode = DRAG_ITEM_MOVE;
            app->_drag_target_idx = i;
            app->_drag_item_idx = item_idx;
            app->_dragging_item = inv->_items._data[item_idx];
            return;
        }
    }
}

static void _handle_mouse_drag( app_state_t* app, int mx, int my ) {
    if( app->_drag_mode == DRAG_WINDOW_MOVE ) {
        inventory_t* inv = &app->_inventories[app->_drag_target_idx];
        inv->_rect.x = mx - app->_drag_offset._x;
        inv->_rect.y = my - app->_drag_offset._y;
        _app_check_window_collision( app, app->_drag_target_idx );
    }
    else if( app->_drag_mode == DRAG_WINDOW_RESIZE ) {
        inventory_t* inv = &app->_inventories[app->_drag_target_idx];
        int new_w = mx - inv->_rect.x + 1;
        int new_h = my - inv->_rect.y + 1;
        
        if( new_w < 15 ) new_w = 15;
        int min_h = _inv_get_calc_height( inv );
        if( new_h < min_h ) new_h = min_h;

        rect_t test = inv->_rect;
        test.w = new_w; 
        test.h = new_h;
        
        if( !_app_check_collision( app, app->_drag_target_idx, test ) ) {
            inv->_rect.w = new_w;
            inv->_rect.h = new_h;
        }
    }
    else if( app->_drag_mode == DRAG_ITEM_MOVE ) {
        for( int i = 0; i < app->_inv_count; ++i ) {
            app->_inventories[i]._is_green_border = false;
            if( i == app->_drag_target_idx ) continue;

            rect_t check = app->_inventories[i]._rect;
            if( app->_view_mode == VIEW_NORMAL ) check.h = _inv_get_calc_height( &app->_inventories[i] );

            if( _rect_contains( check, mx, my ) ) {
                app->_inventories[i]._is_green_border = true;
            }
        }
    }
}

// C-Style clamp (int)
static int _clamp_i( int v, int min, int max ) {
    if( v < min ) return min;
    if( v > max ) return max;
    return v;
}

static void _handle_mouse_release( app_state_t* app ) {
    if( app->_drag_mode == DRAG_WINDOW_MOVE ) {
        inventory_t* inv = &app->_inventories[app->_drag_target_idx];

        if( inv->_rect.x == app->_drag_start_win_pos._x && inv->_rect.y == app->_drag_start_win_pos._y ) {
            _list_sort( &inv->_items );
            snprintf( app->_log_msg, sizeof(app->_log_msg), "Items Sorted: %s", inv->_title );
        } else {
            cc_term_size_t size = cc_screen_get_size();
            int cx = _clamp_i( inv->_rect.x, 1, size._cols - inv->_rect.w );
            int cy = _clamp_i( inv->_rect.y, 2, size._rows - inv->_rect.h - 1 );
            
            inv->_rect.x = cx; 
            inv->_rect.y = cy;

            if( _app_check_collision( app, app->_drag_target_idx, inv->_rect ) ) {
                inv->_is_red_border = true;
            } else {
                inv->_is_red_border = false;
            }

            if( inv->_is_red_border ) {
                cc_coord_t valid = _app_find_valid_pos( app, app->_drag_target_idx );
                inv->_rect.x = valid._x;
                inv->_rect.y = valid._y;
                inv->_is_red_border = false;
            }
        }
    }
    else if( app->_drag_mode == DRAG_ITEM_MOVE ) {
        for( int i = 0; i < app->_inv_count; ++i ) app->_inventories[i]._is_green_border = false;

        for( int i = 0; i < app->_inv_count; ++i ) {
            if( i == app->_drag_target_idx ) continue;
            
            rect_t check = app->_inventories[i]._rect;
            if( app->_view_mode == VIEW_NORMAL ) check.h = _inv_get_calc_height( &app->_inventories[i] );
            
            if( _rect_contains( check, app->_mouse_cursor._x, app->_mouse_cursor._y ) ) {
                _list_add( &app->_inventories[i]._items, app->_dragging_item );
                _list_remove_at( &app->_inventories[app->_drag_target_idx]._items, app->_drag_item_idx );
                snprintf( app->_log_msg, sizeof(app->_log_msg), "Moved to %s", app->_inventories[i]._title );
                break;
            }
        }
    }
    
    app->_drag_mode = DRAG_NONE;
    app->_drag_target_idx = -1;
    app->_drag_item_idx = -1;
}

static void _process_input( app_state_t* app, const cc_input_event_t* ev ) {
    if( ev->_code == CC_KEY_MOUSE_EVENT ) {
        app->_mouse_cursor._x = ev->_data._mouse._x - 1;
        app->_mouse_cursor._y = ev->_data._mouse._y - 1;
    }

    if( ev->_code == (cc_key_code_e)'q' ) { app->_is_running = false; return; }
    if( ev->_code == CC_KEY_F1 ) {
        if( app->_view_mode == VIEW_NORMAL ) {
            _app_save_layout( app ); _app_apply_maximized( app ); 
            app->_view_mode = VIEW_MAXIMIZED; strcpy(app->_log_msg, "Mode: Maximized");
        }
    }
    if( ev->_code == CC_KEY_F2 ) {
        if( app->_view_mode == VIEW_MAXIMIZED ) {
            _app_restore_layout( app ); app->_view_mode = VIEW_NORMAL; 
            strcpy(app->_log_msg, "Mode: Normal");
        }
    }

    if( ev->_code == CC_KEY_MOUSE_EVENT ) {
        int mx = app->_mouse_cursor._x;
        int my = app->_mouse_cursor._y;
        
        if( ev->_data._mouse._button == CC_MOUSE_BTN_LEFT ) {
            if( ev->_data._mouse._action == CC_MOUSE_ACTION_PRESS ) {
                _handle_mouse_press( app, mx, my );
            }
        }
        
        if( ev->_data._mouse._action == CC_MOUSE_ACTION_DRAG ) {
            _handle_mouse_drag( app, mx, my );
        }
        else if( ev->_data._mouse._action == CC_MOUSE_ACTION_RELEASE ) {
            _handle_mouse_release( app );
        }
    }
}

// -----------------------------------------------------------------------------
// Rendering
// -----------------------------------------------------------------------------

static void _render( app_state_t* app ) {
    // 1. Update Heights (Layout)
    for( int i = 0; i < app->_inv_count; ++i ) {
        bool resizing = ( app->_drag_mode == DRAG_WINDOW_RESIZE && app->_drag_target_idx == i );
        if( app->_view_mode == VIEW_NORMAL && !resizing ) {
            _inv_update_height( &app->_inventories[i] );
        }
    }

    // 2. Prepare Buffer
    cc_term_size_t size = cc_screen_get_size();
    cc_buffer_resize( app->_screen_buffer, size._cols, size._rows );
    cc_buffer_clear( app->_screen_buffer, &CC_COLOR_BLACK );

    // 3. Draw Inventories
    for( int i = 0; i < app->_inv_count; ++i ) {
        _inv_draw( &app->_inventories[i], app->_screen_buffer );
    }

    // 4. Draw UI
    // Top Bar
    cc_color_t blue = CC_COLOR_BLUE; cc_color_t white = CC_COLOR_WHITE;
    for( int x = 0; x < size._cols; ++x ) cc_buffer_draw_string( app->_screen_buffer, x, 0, " ", &white, &blue );
    
    int cx = 1;
    cc_buffer_draw_string( app->_screen_buffer, cx, 0, " [Q]uit ", &white, &blue ); cx += 8;
    cc_buffer_draw_string( app->_screen_buffer, cx++, 0, "|", &white, &blue );
    cc_buffer_draw_string( app->_screen_buffer, cx, 0, " [F1] Max ", &white, &blue ); cx += 10;
    cc_buffer_draw_string( app->_screen_buffer, cx++, 0, "|", &white, &blue );
    cc_buffer_draw_string( app->_screen_buffer, cx, 0, " [F2] Restore ", &white, &blue );
    
    // Bottom Log
    int by = size._rows - 1;
    cc_color_t gray; cc_color_init_rgb(&gray, 40, 40, 40);
    for( int x = 0; x < size._cols; ++x ) cc_buffer_draw_string( app->_screen_buffer, x, by, " ", &white, &gray );
    char log_buf[512];
    snprintf( log_buf, sizeof(log_buf), " Log: %s", app->_log_msg );
    cc_buffer_draw_string( app->_screen_buffer, 1, by, log_buf, &white, &gray );

    // 5. Drag Overlay
    if( app->_drag_mode == DRAG_ITEM_MOVE ) {
        int x = app->_mouse_cursor._x + 2;
        int y = app->_mouse_cursor._y + 1;
        char content[MAX_TEXT_LEN + 2];
        snprintf( content, sizeof(content), " %s ", app->_dragging_item.name );
        int w = (int)cc_util_get_string_width( content );
        
        cc_color_t box_bg = CC_COLOR_BLACK; 
        cc_color_t box_fg = CC_COLOR_CYAN;

        // Simple Box
        char top[128] = "┌"; for(int k=0;k<w;k++) strcat(top, "─"); strcat(top, "┐");
        char bot[128] = "└"; for(int k=0;k<w;k++) strcat(bot, "─"); strcat(bot, "┘");

        cc_buffer_draw_string( app->_screen_buffer, x, y, top, &box_fg, &box_bg );
        cc_buffer_draw_string( app->_screen_buffer, x, y+1, "│", &box_fg, &box_bg );
        cc_buffer_draw_string( app->_screen_buffer, x+1, y+1, content, &box_fg, &box_bg );
        cc_buffer_draw_string( app->_screen_buffer, x+1+w, y+1, "│", &box_fg, &box_bg );
        cc_buffer_draw_string( app->_screen_buffer, x, y+2, bot, &box_fg, &box_bg );
    }

    cc_buffer_flush( app->_screen_buffer );
}

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------

int main( void ) {
    // Init App State
    app_state_t app;
    memset( &app, 0, sizeof(app_state_t) );
    
    app._is_running = true;
    app._need_render = true;
    app._view_mode = VIEW_NORMAL;
    app._drag_mode = DRAG_NONE;
    app._drag_target_idx = -1;
    app._drag_item_idx = -1;
    strcpy( app._log_msg, "Ready" );

    // Create Inventories
    app._inv_count = 5;
    app._inventories = (inventory_t*)malloc( sizeof(inventory_t) * app._inv_count );
    
    int items_per_inv[] = { 4, 3, 2, 1, 0 };
    int total_cnt = 0;

    for( int i = 0; i < app._inv_count; ++i ) {
        char title[64];
        snprintf( title, sizeof(title), "Inventory %c", 'A' + i );
        _inv_init( &app._inventories[i], title, 2 + (i * 32), 5, 30 );
        
        for( int k = 0; k < items_per_inv[i]; ++k ) {
            total_cnt++;
            item_t item;
            snprintf( item.name, sizeof(item.name), "Equipment_No.%d", total_cnt );
            snprintf( item.desc, sizeof(item.desc), "Desc for %s", item.name );
            _list_add( &app._inventories[i]._items, item );
        }
    }
    _app_save_layout( &app );

    // Console Init
    cc_device_init();
    cc_device_enable_mouse( true );
    cc_screen_set_back_color( &CC_COLOR_BLACK );
    cc_screen_clear();
    
    app._screen_buffer = cc_buffer_create( 80, 24 );

    // Main Loop
    while( app._is_running ) {
        cc_key_code_e key = cc_device_get_input( 10 );
        if( key != CC_KEY_NONE ) {
            cc_input_event_t evt;
            cc_device_inspect( key, &evt );
            _process_input( &app, &evt );
            app._need_render = true;
        }

        if( app._need_render ) {
            _render( &app );
            app._need_render = false;
        }
    }

    // Cleanup
    cc_device_enable_mouse( false );
    cc_screen_clear();

    for( int i = 0; i < app._inv_count; ++i ) {
        _list_free( &app._inventories[i]._items );
    }
    free( app._inventories );
    cc_buffer_destroy( app._screen_buffer );

    cc_device_deinit();
    return 0;
}