/**
 * pgmspace.h - PROGMEM compatibility for ESP-IDF
 *
 * ESP32 has a unified address space, so PROGMEM is not needed.
 * These macros make PROGMEM code compile without modification.
 */
#ifndef PGMSPACE_H_
#define PGMSPACE_H_

#include <stdint.h>
#include <string.h>

// PROGMEM is not needed on ESP32 (unified address space)
#ifndef PROGMEM
#define PROGMEM
#endif

#ifndef PGM_P
#define PGM_P const char *
#endif

#ifndef PGM_VOID_P
#define PGM_VOID_P const void *
#endif

// pgm_read_* macros - just dereference the pointer
#ifndef pgm_read_byte
#define pgm_read_byte(addr)   (*(const uint8_t *)(addr))
#endif
#ifndef pgm_read_word
#define pgm_read_word(addr)   (*(const uint16_t *)(addr))
#endif
#ifndef pgm_read_dword
#define pgm_read_dword(addr)  (*(const uint32_t *)(addr))
#endif
#ifndef pgm_read_float
#define pgm_read_float(addr)  (*(const float *)(addr))
#endif
#ifndef pgm_read_ptr
#define pgm_read_ptr(addr)    (*(const void * const *)(addr))
#endif

// pgm_read with near/far variants (all the same on ESP32)
#define pgm_read_byte_near(addr)  pgm_read_byte(addr)
#define pgm_read_word_near(addr)  pgm_read_word(addr)
#define pgm_read_dword_near(addr) pgm_read_dword(addr)
#define pgm_read_byte_far(addr)   pgm_read_byte(addr)
#define pgm_read_word_far(addr)   pgm_read_word(addr)
#define pgm_read_dword_far(addr)  pgm_read_dword(addr)

// String functions for PROGMEM (just map to standard functions)
#define memcpy_P    memcpy
#define memcmp_P    memcmp
#define strlen_P    strlen
#define strcpy_P    strcpy
#define strncpy_P   strncpy
#define strcmp_P     strcmp
#define strncmp_P   strncmp
#define strcat_P    strcat
#define strchr_P    strchr
#define strstr_P    strstr
#define sprintf_P   sprintf
#define snprintf_P  snprintf
#define vsnprintf_P vsnprintf

// PSTR - store string in flash (no-op on ESP32)
#ifndef PSTR
#define PSTR(s) (s)
#endif

// F() macro - store string in flash (no-op on ESP32)
class __FlashStringHelper;
#define F(string_literal) (reinterpret_cast<const __FlashStringHelper *>(PSTR(string_literal)))
#define FPSTR(pstr_pointer) (reinterpret_cast<const __FlashStringHelper *>(pstr_pointer))

#endif // PGMSPACE_H_
