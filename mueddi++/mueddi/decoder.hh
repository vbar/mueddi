#ifndef mueddi_decoder_hh
#define mueddi_decoder_hh

#include <stdint.h>
#include <stddef.h>

// Copyright (c) 2008-2009 Bjoern Hoehrmann <bjoern@hoehrmann.de>
// See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.

#define UTF8_ACCEPT 0
#define UTF8_REJECT 1

namespace mueddi
{

uint32_t decode(uint32_t *state, uint32_t *codep, uint32_t byte);

size_t get_code_point_count(const unsigned char *s);

}

#endif
