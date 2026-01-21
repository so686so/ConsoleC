#ifndef _CONSOLE_C_H_
#define _CONSOLE_C_H_

/** ------------------------------------------------------------------------------------
 * ConsoleC Integrated Header
 * ------------------------------------------------------------------------------------
 * ConsoleC 라이브러리의 모든 기능을 포함하는 단일 헤더 파일입니다.
 * ------------------------------------------------------------------------------------ */

#ifdef __cplusplus
extern "C" {
#endif

// Core Modules
#include "console_c/cc_color.h"
#include "console_c/cc_util.h"
#include "console_c/cc_screen.h" // Includes cc_device definitions (Types)
#include "console_c/cc_device.h"
#include "console_c/cc_buffer.h"

#ifdef __cplusplus
}
#endif

#endif // _CONSOLE_C_H_