#ifndef mueddi_encoder_hh
#define mueddi_encoder_hh

#include <stdint.h>
#include <stddef.h>

/**
 * Encode a code point using UTF-8
 *
 * @author Ondřej Hruška <ondra@ondrovo.com>
 * @license MIT
 *
 * @param out - output buffer (min 5 characters), will be 0-terminated
 * @param utf - code point 0-0x10FFFF
 * @return number of bytes on success, 0 on failure (also produces U+FFFD, which uses 3 bytes)
 */

namespace mueddi
{

size_t utf8_encode(char *out, uint32_t utf);

}

#endif
