#ifndef MATRA_LEXER_LEXER_H
#define MATRA_LEXER_LEXER_H

#include "../source/source.h"
#include "token.h"

#include <stdbool.h>

bool lexer_scan(const SourceBuffer *source, TokenArray *tokens,
                Diagnostics *diagnostics);

#endif
