#ifndef MATRA_PARSER_PARSER_H
#define MATRA_PARSER_PARSER_H

#include "../ast/ast.h"

#include <stdbool.h>

bool parser_parse(AstArena *arena, const TokenArray *tokens,
                 Diagnostics *diagnostics, AstNode **program);

#endif
