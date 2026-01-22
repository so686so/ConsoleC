/** ------------------------------------------------------------------------------------
 * ConsoleC Device Input Module Implementation
 * ------------------------------------------------------------------------------------
 * cc_device.h 의 구현부입니다.
 * POSIX Thread, Select, Termios를 사용합니다.
 * ------------------------------------------------------------------------------------ */

// Feature Test Macros (for pthreads, time, etc.)
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "console_c/cc_device.h"
#include "console_c/cc_screen.h" // GetSize 구현 시 필요

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sys/select.h>
#include <sys/eventfd.h>
#include <sys/time.h>
#include <sys/ioctl.h>

// -----------------------------------------------------------------------------
// Internal State (Static Singleton Replacement)
// -----------------------------------------------------------------------------

static int               g_event_fd = -1;
static struct termios    g_orig_termios;
static atomic_bool       g_is_raw_mode       = ATOMIC_VAR_INIT(false);
static atomic_bool       g_is_mouse_tracking = ATOMIC_VAR_INIT(false);
static atomic_bool       g_is_input_running  = ATOMIC_VAR_INIT(false);

static struct sigaction  g_old_sa_winch;
static struct sigaction  g_old_sa_int;

// Input Buffer
static char              g_input_buf[512];
static size_t            g_input_len = 0;

// Last Known States
static cc_mouse_state_t  g_last_mouse_state = {0};
static cc_coord_t        g_last_cursor_pos  = {0, 0};

// Cursor Request Sync (Condition Variable)
static pthread_mutex_t   g_cursor_req_mtx  = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t    g_cursor_req_cond = PTHREAD_COND_INITIALIZER;
static bool              g_cursor_req_pending = false;
static cc_coord_t        g_cursor_req_result  = {0, 0};

// Internal Constants
static const uint64_t EVENT_CODE_INTERRUPT = 1;
static const uint64_t EVENT_CODE_RESIZE    = 2;

// -----------------------------------------------------------------------------
// Internal Function Prototypes
// -----------------------------------------------------------------------------
static void _set_raw_mode( bool enable );
static void _reset_terminal_mode( void );
static void _handle_signal( int sig );
static cc_key_code_e _parse_input_buffer( size_t* out_consumed );
static cc_key_code_e _parse_mouse_sequence( const char* buf, size_t len, size_t* out_consumed );

// -----------------------------------------------------------------------------
// Public API Implementation
// -----------------------------------------------------------------------------

void cc_device_init( void )
{
    // Create EventFD
    g_event_fd = eventfd( 0, EFD_NONBLOCK | EFD_CLOEXEC );
    if( g_event_fd == -1 ) {
        perror("cc_device_init: eventfd failed");
    }

    // Register Signal Handlers
    struct sigaction sa;
    memset( &sa, 0, sizeof(sa) );
    sa.sa_handler = _handle_signal;
    sigemptyset( &sa.sa_mask );
    // SA_RESTART를 쓰면 read가 안 깨질 수 있으므로 제거하거나 주의

    sigaction( SIGWINCH, &sa, &g_old_sa_winch );
    sigaction( SIGINT,   &sa, &g_old_sa_int );

    // Save Original Termios
    if( tcgetattr( STDIN_FILENO, &g_orig_termios ) == -1 ) {
        perror("cc_device_init: tcgetattr failed");
    }

    // Enable Raw Mode
    cc_device_resume();
}

void cc_device_deinit( void )
{
    cc_device_enable_mouse( false );

    _set_raw_mode( false ); // Restore terminal

    // Restore Signals
    sigaction( SIGWINCH, &g_old_sa_winch, NULL );
    sigaction( SIGINT,   &g_old_sa_int,   NULL );

    if( g_event_fd != -1 ) {
        close( g_event_fd );
        g_event_fd = -1;
    }
}

void cc_device_force_pause( void )
{
    // Wake up any blocking select
    if( g_event_fd != -1 ) {
        uint64_t u = EVENT_CODE_INTERRUPT;
        if( write( g_event_fd, &u, sizeof(uint64_t) ) < 0 ) {}
    }
    _set_raw_mode( false );
}

void cc_device_resume( void )
{
    _set_raw_mode( true );
}

void cc_device_enable_mouse( bool enable )
{
    atomic_store( &g_is_mouse_tracking, enable );

    if( enable ) {
        const char* seq = "\033[?1000h\033[?1002h\033[?1006h";
        if( write( STDOUT_FILENO, seq, strlen(seq) ) < 0 ) {}
    } else {
        const char* seq = "\033[?1000l\033[?1002l\033[?1006l";
        if( write( STDOUT_FILENO, seq, strlen(seq) ) < 0 ) {}
    }
}

cc_mouse_state_t cc_device_get_mouse_state( void )
{
    return g_last_mouse_state;
}

cc_key_code_e cc_device_get_input( int timeout_ms )
{
    // 1. Thread Safety Gatekeeper
    bool expected = false;
    if( !atomic_compare_exchange_strong( &g_is_input_running, &expected, true ) ) {
        return CC_KEY_BUSY;
    }

    // Ensure Raw Mode
    if( !atomic_load( &g_is_raw_mode ) ) {
        _set_raw_mode( true );
    }

    cc_key_code_e result_key = CC_KEY_NONE;
    struct timespec start_ts, curr_ts;

    if( timeout_ms > 0 ) {
        clock_gettime( CLOCK_MONOTONIC, &start_ts );
    }

    while( true ) {
        // A. Parse Buffer First
        if( g_input_len > 0 ) {
            size_t consumed = 0;
            cc_key_code_e key = _parse_input_buffer( &consumed );

            if( consumed > 0 ) {
                // Remove consumed data
                if( consumed < g_input_len ) {
                    memmove( g_input_buf, g_input_buf + consumed, g_input_len - consumed );
                }
                g_input_len -= consumed;

                // [Intercept Cursor Event]
                if( key == CC_KEY_CURSOR_EVENT ) {
                    pthread_mutex_lock( &g_cursor_req_mtx );
                    if( g_cursor_req_pending ) {
                        g_cursor_req_result  = g_last_cursor_pos;
                        g_cursor_req_pending = false;
                        pthread_cond_signal( &g_cursor_req_cond );
                    }
                    pthread_mutex_unlock( &g_cursor_req_mtx );

                    // User shouldn't see this internal event
                    continue;
                }

                if( key != CC_KEY_NONE ) {
                    result_key = key;
                    break; // Found Key!
                }
            }
            // If consumed == 0, incomplete data. Need to read more.
        }

        // B. Calculate Timeout
        struct timeval tv;
        struct timeval* ptv = NULL;

        if( timeout_ms >= 0 ) {
            long remaining = timeout_ms;
            if( timeout_ms > 0 ) {
                clock_gettime( CLOCK_MONOTONIC, &curr_ts );
                long elapsed_ms = (curr_ts.tv_sec - start_ts.tv_sec) * 1000 +
                                  (curr_ts.tv_nsec - start_ts.tv_nsec) / 1000000;
                remaining = timeout_ms - elapsed_ms;
            }

            if( remaining <= 0 ) {
                result_key = CC_KEY_NONE; // Timeout
                break;
            }

            tv.tv_sec  = remaining / 1000;
            tv.tv_usec = ( remaining % 1000 ) * 1000;
            ptv = &tv;
        }

        // C. Select
        fd_set readfds;
        FD_ZERO( &readfds );
        FD_SET( STDIN_FILENO, &readfds );
        if( g_event_fd != -1 ) FD_SET( g_event_fd, &readfds );

        int max_fd = ( g_event_fd > STDIN_FILENO ) ? g_event_fd : STDIN_FILENO;

        int ret = select( max_fd + 1, &readfds, NULL, NULL, ptv );

        if( ret < 0 ) {
            if( errno == EINTR ) continue; // Signal caught, retry
            break; // Error
        }
        if( ret == 0 ) {
            // 타임아웃이 났는데 버퍼에 ESC(27) 하나만 남아있다면,
            // 이는 시퀀스가 아니라 사용자가 ESC 키를 누른 것이다.
            if( g_input_len == 1 && g_input_buf[0] == 27 ) {
                g_input_len = 0; // 버퍼 비움
                result_key = CC_KEY_ESC;
                break;
            }

            result_key = CC_KEY_NONE; // Timeout
            break;
        }

        // D. Read Data
        bool data_read = false;

        // D-1. EventFD
        if( g_event_fd != -1 && FD_ISSET( g_event_fd, &readfds ) ) {
            uint64_t u = 0;
            if( read( g_event_fd, &u, sizeof(u) ) > 0 ) {
                if( u == EVENT_CODE_INTERRUPT ) { result_key = CC_KEY_INTERRUPT; break; }
                if( u == EVENT_CODE_RESIZE )    { result_key = CC_KEY_RESIZE_EVENT; break; }
                data_read = true;
            }
        }

        // D-2. STDIN
        if( FD_ISSET( STDIN_FILENO, &readfds ) ) {
            if( g_input_len < sizeof(g_input_buf) ) {
                ssize_t len = read( STDIN_FILENO, g_input_buf + g_input_len, sizeof(g_input_buf) - g_input_len );
                if( len > 0 ) {
                    g_input_len += len;
                    data_read = true;
                }
            }
        }

        if( data_read ) continue; // Loop back to parsing
    }

    // Release Gatekeeper
    atomic_store( &g_is_input_running, false );
    return result_key;
}

bool cc_device_get_cursor_pos( int timeout_ms, cc_coord_t* out_coord )
{
    // 1. Send Request
    if( write( STDOUT_FILENO, "\033[6n", 4 ) < 0 ) return false;

    // 2. Check who owns input loop
    if( atomic_load( &g_is_input_running ) ) {
        // [Observer Mode] Wait for signal
        struct timespec ts;
        clock_gettime( CLOCK_REALTIME, &ts ); // cond_wait uses REALTIME usually

        // Add MS
        ts.tv_sec  += timeout_ms / 1000;
        ts.tv_nsec += ( timeout_ms % 1000 ) * 1000000;
        if( ts.tv_nsec >= 1000000000 ) {
            ts.tv_sec += 1;
            ts.tv_nsec -= 1000000000;
        }

        pthread_mutex_lock( &g_cursor_req_mtx );
        g_cursor_req_pending = true;

        int rc = pthread_cond_timedwait( &g_cursor_req_cond, &g_cursor_req_mtx, &ts );

        bool success = false;
        if( rc == 0 && !g_cursor_req_pending ) { // Signaled and flag cleared
            if( out_coord ) *out_coord = g_cursor_req_result;
            success = true;
        } else {
            g_cursor_req_pending = false; // Clean up on timeout
        }

        pthread_mutex_unlock( &g_cursor_req_mtx );
        return success;
    } else {
        // [Direct Mode] Read manually
        // We act as the input loop for a moment.
        struct timespec start_ts, curr_ts;
        clock_gettime( CLOCK_MONOTONIC, &start_ts );

        while( true ) {
            // Calc remaining time
            clock_gettime( CLOCK_MONOTONIC, &curr_ts );
            long elapsed = (curr_ts.tv_sec - start_ts.tv_sec) * 1000 +
                           (curr_ts.tv_nsec - start_ts.tv_nsec) / 1000000;
            int remaining = timeout_ms - (int)elapsed;
            if( remaining <= 0 ) return false;

            // Recurse (Since g_is_input_running is false, we can enter)
            cc_key_code_e key = cc_device_get_input( remaining );

            if( key == CC_KEY_CURSOR_EVENT ) {
                // Wait! cc_device_get_input logic handles CURSOR_EVENT internally
                // by signaling cond. In Direct Mode, we need access to the parsed data.
                // However, our get_input logic SWALLOWS CURSOR_EVENT if pending is set.
                // Wait... if pending is FALSE, get_input returns CC_KEY_CURSOR_EVENT?
                // Let's check get_input logic:
                // "if( key == CC_KEY_CURSOR_EVENT ) { ... continue; }"
                // It unconditionally swallows it if parsed.

                // Correction: In Direct Mode, get_input will update g_last_cursor_pos
                // inside _parse_input_buffer. But the loop in get_input will swallow the key
                // and continue. So get_input might return NONE or Timeout if we only wait for cursor.

                // Issue: cc_device_get_input design hides cursor events.
                // Fix: Check g_last_cursor_pos directly or trust internal state update?
                // Actually, since get_input swallows it, it will return Timeout eventually if only cursor event comes.

                // Better approach for Direct Mode:
                // Just use the Observer logic mechanism even for Direct Mode!
                // We just need to start a separate thread or just call get_input and check the Condition variable?
                // No, calling get_input consumes the buffer.

                // Let's tweak get_input slightly:
                // If it sees Cursor Event, it updates g_cursor_req_result/Signal.
                // So in Direct Mode, we just need to set the Pending flag and call get_input!
            }

            // Revised Direct Mode Strategy:
            // 1. Set Pending Flag.
            // 2. Call get_input.
            // 3. get_input will see the event, signal the cond, and swallow it.
            // 4. We check the result.

            pthread_mutex_lock( &g_cursor_req_mtx );
            g_cursor_req_pending = true;
            pthread_mutex_unlock( &g_cursor_req_mtx );

            cc_device_get_input( remaining );

            pthread_mutex_lock( &g_cursor_req_mtx );
            bool got_it = !g_cursor_req_pending; // If cleared, it was processed
            if( got_it && out_coord ) *out_coord = g_cursor_req_result;
            g_cursor_req_pending = false; // reset
            pthread_mutex_unlock( &g_cursor_req_mtx );

            if( got_it ) return true;
        }
    }
}

void cc_device_inspect( cc_key_code_e key_code, cc_input_event_t* out_event )
{
    if( !out_event ) return;

    out_event->_code = key_code;

    switch( key_code ) {
        case CC_KEY_MOUSE_EVENT:
            out_event->_data._mouse = g_last_mouse_state;
            break;
        case CC_KEY_RESIZE_EVENT:
            // Need cc_screen to get size, but avoid circular dep if possible.
            // Assuming cc_screen_get_size is available or use ioctl directly here.
            {
                struct winsize ws;
                if( ioctl( STDOUT_FILENO, TIOCGWINSZ, &ws ) != -1 ) {
                    out_event->_data._term_size._cols = ws.ws_col;
                    out_event->_data._term_size._rows = ws.ws_row;
                }
            }
            break;
        case CC_KEY_CURSOR_EVENT:
            out_event->_data._cursor = g_last_cursor_pos;
            break;
        default:
            break;
    }
}

const char* cc_device_key_to_string( cc_key_code_e key )
{
    static char buf[32];
    if( key >= CC_KEY_SPACE && key <= 126 ) {
        snprintf(buf, sizeof(buf), "%c", (char)key);
        return buf;
    }
    switch( key ) {
        case CC_KEY_TAB:       return "TAB";
        case CC_KEY_ENTER:     return "ENTER";
        case CC_KEY_ESC:       return "ESC";
        case CC_KEY_SPACE:     return "SPACE";
        case CC_KEY_BACKSPACE: return "BACKSPACE";

        case CC_KEY_UP:        return "UP";
        case CC_KEY_DOWN:      return "DOWN";
        case CC_KEY_LEFT:      return "LEFT";
        case CC_KEY_RIGHT:     return "RIGHT";

        case CC_KEY_INSERT:    return "INSERT";
        case CC_KEY_DEL:       return "DELETE";
        case CC_KEY_HOME:      return "HOME";
        case CC_KEY_END:       return "END";
        case CC_KEY_PAGE_UP:   return "PAGEUP";
        case CC_KEY_PAGE_DOWN: return "PAGEDOWN";

        case CC_KEY_F1:  return "F1";
        case CC_KEY_F2:  return "F2";
        case CC_KEY_F3:  return "F3";
        case CC_KEY_F4:  return "F4";
        case CC_KEY_F5:  return "F5";
        case CC_KEY_F6:  return "F6";
        case CC_KEY_F7:  return "F7";
        case CC_KEY_F8:  return "F8";
        case CC_KEY_F9:  return "F9";
        case CC_KEY_F10: return "F10";
        case CC_KEY_F11: return "F11";
        case CC_KEY_F12: return "F12";

        case CC_KEY_MOUSE_EVENT:  return "MOUSE";
        case CC_KEY_RESIZE_EVENT: return "RESIZE";

        default: snprintf(buf, sizeof(buf), "(%d)", key); return buf;
    }
}

// -----------------------------------------------------------------------------
// Internal Implementation
// -----------------------------------------------------------------------------

static void _set_raw_mode( bool enable )
{
    if( atomic_load( &g_is_raw_mode ) == enable ) return;

    if( enable ) {
        struct termios raw = g_orig_termios;
        raw.c_lflag &= ~( ECHO | ICANON );
        raw.c_cc[VMIN] = 0;  // We use select for timeout, so non-blocking read is fine
        raw.c_cc[VTIME] = 0;
        tcsetattr( STDIN_FILENO, TCSANOW, &raw );

        const char* seq = "\033[?25l";
        if( write( STDOUT_FILENO, seq, strlen(seq) ) < 0 ) {}
        atomic_store( &g_is_raw_mode, true );
    } else {
        tcsetattr( STDIN_FILENO, TCSANOW, &g_orig_termios );
        const char* seq = "\033[?25h";
        if( write( STDOUT_FILENO, seq, strlen(seq) ) < 0 ) {}
        atomic_store( &g_is_raw_mode, false );
    }
}

static void _handle_signal( int sig )
{
    if( sig == SIGWINCH && g_event_fd != -1 ) {
        uint64_t val = EVENT_CODE_RESIZE;
        if( write( g_event_fd, &val, sizeof(val) ) < 0 ) {}
    } else if( sig == SIGINT ) {
        // Restore immediately
        _reset_terminal_mode();
        printf("\n");
        exit(0);
    }
}

static void _reset_terminal_mode( void )
{
    if( atomic_load( &g_is_mouse_tracking ) ) {
        const char* seq = "\033[?1000l\033[?1002l\033[?1006l";
        if( write( STDOUT_FILENO, seq, strlen(seq) ) < 0 ) {}
    }
    tcsetattr( STDIN_FILENO, TCSANOW, &g_orig_termios );
    const char* seq = "\033[?25h";
    if( write( STDOUT_FILENO, seq, strlen(seq) ) < 0 ) {}
}

static cc_key_code_e _parse_input_buffer( size_t* out_consumed )
{
    *out_consumed = 0;
    if( g_input_len == 0 ) return CC_KEY_NONE;

    unsigned char c = (unsigned char)g_input_buf[0];

    // 1. ESC Sequence (\033...)
    if( c == 27 ) {
        if( g_input_len < 2 ) return CC_KEY_NONE; // Incomplete

        // 1-A. CSI Sequence (\033[...)
        if( g_input_buf[1] == '[' ) {
            if( g_input_len < 3 ) return CC_KEY_NONE; // Incomplete

            // Mouse Event (\033[<...)
            if( g_input_buf[2] == '<' ) {
                return _parse_mouse_sequence( g_input_buf, g_input_len, out_consumed );
            }

            // Find Terminator (@..~)
            size_t t_pos = 0;
            for( size_t i = 2; i < g_input_len; ++i ) {
                char ch = g_input_buf[i];
                if( ch >= 0x40 && ch <= 0x7E ) { t_pos = i; break; }
            }
            if( t_pos == 0 ) return CC_KEY_NONE; // Incomplete

            *out_consumed = t_pos + 1;
            char term = g_input_buf[t_pos];

            // Cursor Pos (\033[row;colR)
            if( term == 'R' ) {
                int r=0, c=0;
                if( sscanf( g_input_buf, "\033[%d;%dR", &r, &c ) == 2 ) {
                    // ANSI(1-based) -> User(0-based) 변환
                    g_last_cursor_pos._x = c - 1;
                    g_last_cursor_pos._y = r - 1;
                    return CC_KEY_CURSOR_EVENT;
                }
                return CC_KEY_NONE;
            }

            // Extended Keys (~ terminator)
            if( term == '~' ) {
                int code = 0;
                sscanf( g_input_buf, "\033[%d~", &code );
                switch( code ) {
                    // [누락 수정] F1 ~ F4 (Tera Term 등 일부 터미널 호환)
                    case 11: return CC_KEY_F1;
                    case 12: return CC_KEY_F2;
                    case 13: return CC_KEY_F3;
                    case 14: return CC_KEY_F4;

                    // Existing F-Keys
                    case 15: return CC_KEY_F5;
                    case 17: return CC_KEY_F6;
                    case 18: return CC_KEY_F7;
                    case 19: return CC_KEY_F8;
                    case 20: return CC_KEY_F9;
                    case 21: return CC_KEY_F10;
                    case 23: return CC_KEY_F11;
                    case 24: return CC_KEY_F12;

                    // Navigation
                    case 1:  return CC_KEY_HOME; // Some variants use 1~
                    case 2:  return CC_KEY_INSERT;
                    case 3:  return CC_KEY_DEL;
                    case 4:  return CC_KEY_END;  // Some variants use 4~
                    case 5:  return CC_KEY_PAGE_UP;
                    case 6:  return CC_KEY_PAGE_DOWN;
                    default: return CC_KEY_NONE;
                }
            }

            // ANSI Chars (\033[A ...)
            switch( g_input_buf[2] ) {
                case 'A': return CC_KEY_UP;
                case 'B': return CC_KEY_DOWN;
                case 'C': return CC_KEY_RIGHT;
                case 'D': return CC_KEY_LEFT;
                case 'H': return CC_KEY_HOME;
                case 'F': return CC_KEY_END;
            }
        }
        // 1-B. SS3 Sequence (\033O...)
        else if( g_input_buf[1] == 'O' ) {
             if( g_input_len < 3 ) return CC_KEY_NONE;
             *out_consumed = 3;
             switch( g_input_buf[2] ) {
                 case 'P': return CC_KEY_F1;
                 case 'Q': return CC_KEY_F2;
                 case 'R': return CC_KEY_F3;
                 case 'S': return CC_KEY_F4;

                 // [누락 수정] SS3 모드에서의 Home/End
                 case 'H': return CC_KEY_HOME;
                 case 'F': return CC_KEY_END;
             }
        }

        // Plain ESC
        *out_consumed = 1;
        return CC_KEY_ESC;
    }

    // 2. Normal Char & Control Char
    *out_consumed = 1;
    if( c == 127 || c == 8 ) return CC_KEY_BACKSPACE;
    if( c == 10 || c == 13 ) return CC_KEY_ENTER;
    if( c == 9 )             return CC_KEY_TAB;
    if( c == 32 )            return CC_KEY_SPACE; // SPACE 명시

    // ASCII 범위 내 문자 반환
    return (cc_key_code_e)c;
}

static cc_key_code_e _parse_mouse_sequence( const char* buf, size_t len, size_t* out_consumed )
{
    // Format: \033[<btn;x;yM or m
    // Find Terminator
    size_t end = 0;
    for( size_t i = 3; i < len; ++i ) {
        if( buf[i] == 'M' || buf[i] == 'm' ) { end = i; break; }
    }
    if( end == 0 ) { *out_consumed = 0; return CC_KEY_NONE; } // Incomplete

    *out_consumed = end + 1;

    int b=0, x=0, y=0;
    char type = buf[end];

    // Parse using sscanf is risky with variable digits, simpler to use custom
    // \033[< %d ; %d ; %d %c
    if( sscanf( buf, "\033[<%d;%d;%d", &b, &x, &y ) != 3 ) return CC_KEY_NONE;

    // 핵심: ANSI(1-based) -> User(0-based) 변환
    g_last_mouse_state._x = x - 1;
    g_last_mouse_state._y = y - 1;
    g_last_mouse_state._action = CC_MOUSE_ACTION_UNKNOWN;

    // Logic Mapping
    if( b >= 64 ) {
        g_last_mouse_state._button = CC_MOUSE_BTN_UNKNOWN;
        if( b == 64 ) g_last_mouse_state._action = CC_MOUSE_ACTION_WHEEL_UP;
        else if( b == 65 ) g_last_mouse_state._action = CC_MOUSE_ACTION_WHEEL_DOWN;
    } else {
        // Button
        int btn_code = b & 3;
        switch( btn_code ) {
            case 0: g_last_mouse_state._button = CC_MOUSE_BTN_LEFT; break;
            case 1: g_last_mouse_state._button = CC_MOUSE_BTN_MIDDLE; break;
            case 2: g_last_mouse_state._button = CC_MOUSE_BTN_RIGHT; break;
            default: g_last_mouse_state._button = CC_MOUSE_BTN_UNKNOWN; break;
        }

        if( type == 'm' ) {
            g_last_mouse_state._action = CC_MOUSE_ACTION_RELEASE;
        } else { // 'M'
            if( b & 32 ) g_last_mouse_state._action = CC_MOUSE_ACTION_DRAG;
            else         g_last_mouse_state._action = CC_MOUSE_ACTION_PRESS;
        }
    }

    return CC_KEY_MOUSE_EVENT;
}

int cc_key_to_int( cc_key_code_e key )
{
    // ASCII 코드 상 '0'(48) ~ '9'(57) 범위인지 확인
    if( key >= CC_KEY_0 && key <= CC_KEY_9 ) {
        return (int)key - (int)CC_KEY_0;
    }

    // 숫자가 아닌 키는 변환 불가
    return -1;
}