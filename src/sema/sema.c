#include "sema.h"

#include "../common/str.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    const AstNode *node;
    const SemaType *type;
    SemaEscape escape;
} TypeEntry;

typedef struct {
    Token name;
    const AstNode *declaration;
    const AstNode *value;
    const SemaType *type;
    size_t scope;
    bool mutable_binding;
} Symbol;

typedef struct {
    Token name;
    const AstNode *declaration;
    SemaType *type;
} NominalType;

typedef struct {
    SemaType *type;
    const SemaType **members;
    size_t member_count;
    const SemaType *base;
} TypeShape;

typedef struct {
    const AstNode *function;
    const SemaType *result;
} FunctionResult;

typedef struct {
    TypeEntry *types;
    size_t type_count;
    size_t type_capacity;
    Symbol *symbols;
    size_t symbol_count;
    size_t symbol_capacity;
    NominalType *nominals;
    size_t nominal_count;
    size_t nominal_capacity;
    TypeShape *shapes;
    size_t shape_count;
    size_t shape_capacity;
    FunctionResult *function_results;
    size_t function_result_count;
    size_t function_result_capacity;
    Token *type_variables;
    size_t type_variable_count;
    size_t type_variable_capacity;
    size_t scope;
    size_t loop_depth;
    size_t function_depth;
    const SemaType *return_type;
    const SemaType *inferred_return_type;
    NominalType *current_class;
    const char *source_name;
    Diagnostics *diagnostics;
} SemaState;

static const SemaType sema_types[] = {
    { SEMA_TYPE_ERROR, "<error>" }, { SEMA_TYPE_UNKNOWN, "<unknown>" },
    { SEMA_TYPE_UNIT, "Unit" }, { SEMA_TYPE_BOOL, "Bool" },
    { SEMA_TYPE_INT, "Int64" }, { SEMA_TYPE_FLOAT, "Float64" },
    { SEMA_TYPE_STRING, "String" }, { SEMA_TYPE_CHAR, "Char" },
    { SEMA_TYPE_FUNCTION, "Function" },
    { SEMA_TYPE_COLLECTION, "Collection" },
    { SEMA_TYPE_NOMINAL, "Nominal" }, { SEMA_TYPE_ADT, "ADT" },
    { SEMA_TYPE_GENERIC, "Generic" }, { SEMA_TYPE_UNION, "Union" }
};

static bool sema_grow(void **items, size_t *capacity, size_t element_size,
                      size_t required) {
    size_t next = *capacity == 0U ? 16U : *capacity;
    void *grown;
    if (required > SIZE_MAX / element_size) return false;
    while (next < required) {
        if (next > SIZE_MAX / 2U) { next = required; break; }
        next *= 2U;
    }
    grown = realloc(*items, next * element_size);
    if (grown == NULL) return false;
    *items = grown;
    *capacity = next;
    return true;
}

static const TypeShape *sema_shape(const SemaState *state,
                                   const SemaType *type) {
    size_t index;
    for (index = 0U; index < state->shape_count; ++index)
        if (state->shapes[index].type == type) return &state->shapes[index];
    return NULL;
}

static SemaType *sema_new_shape(SemaState *state, SemaTypeKind kind,
                               const char *name, const SemaType **members,
                               size_t member_count, const SemaType *base) {
    SemaType *type = (SemaType *)malloc(sizeof(*type));
    char *name_copy = NULL;
    const SemaType **member_copy = NULL;
    size_t name_length;
    size_t shape_index;
    if (type == NULL || name == NULL ||
        (member_count > 0U && members == NULL)) {
        free(type);
        return NULL;
    }
    for (shape_index = 0U; shape_index < state->shape_count; ++shape_index) {
        TypeShape *existing = &state->shapes[shape_index];
        size_t member_index;
        if (existing->type->kind != kind || existing->base != base ||
            existing->member_count != member_count ||
            strcmp(existing->type->name, name) != 0) continue;
        for (member_index = 0U; member_index < member_count; ++member_index)
            if (existing->members[member_index] != members[member_index]) break;
        if (member_index == member_count) {
            free(type);
            return existing->type;
        }
    }
    name_length = strlen(name);
    if (name_length == SIZE_MAX) { free(type); return NULL; }
    name_copy = (char *)malloc(name_length + 1U);
    if (name_copy == NULL) { free(type); return NULL; }
    memcpy(name_copy, name, name_length + 1U);
    if (member_count > 0U) {
        if (member_count > SIZE_MAX / sizeof(*member_copy)) {
            free(name_copy); free(type); return NULL;
        }
        member_copy = (const SemaType **)malloc(member_count * sizeof(*member_copy));
        if (member_copy == NULL) { free(name_copy); free(type); return NULL; }
        memcpy(member_copy, members, member_count * sizeof(*member_copy));
    }
    if (!sema_grow((void **)&state->shapes, &state->shape_capacity,
                   sizeof(*state->shapes), state->shape_count + 1U)) {
        free(member_copy); free(name_copy); free(type); return NULL;
    }
    type->kind = kind;
    type->name = name_copy;
    state->shapes[state->shape_count++] =
        (TypeShape){ type, member_copy, member_count, base };
    return type;
}

static bool sema_type_compatible(const SemaState *state,
                                const SemaType *actual,
                                const SemaType *expected) {
    const TypeShape *actual_shape;
    const TypeShape *expected_shape;
    size_t index;
    if (actual == NULL || expected == NULL ||
        actual->kind == SEMA_TYPE_ERROR || expected->kind == SEMA_TYPE_ERROR ||
        actual->kind == SEMA_TYPE_UNKNOWN || expected->kind == SEMA_TYPE_UNKNOWN)
        return true;
    if (actual == expected) return true;
    expected_shape = sema_shape(state, expected);
    actual_shape = sema_shape(state, actual);
    if (actual->kind == SEMA_TYPE_UNION && actual_shape != NULL) {
        if (expected->kind != SEMA_TYPE_UNION || expected_shape == NULL) return false;
        for (index = 0U; index < actual_shape->member_count; ++index) {
            size_t expected_index;
            bool covered = false;
            for (expected_index = 0U;
                 expected_index < expected_shape->member_count; ++expected_index)
                if (sema_type_compatible(state, actual_shape->members[index],
                                         expected_shape->members[expected_index])) {
                    covered = true; break;
                }
            if (!covered) return false;
        }
        return true;
    }
    if (expected->kind == SEMA_TYPE_UNION && expected_shape != NULL) {
        for (index = 0U; index < expected_shape->member_count; ++index)
            if (sema_type_compatible(state, actual,
                                     expected_shape->members[index])) return true;
        return false;
    }
    if (actual->kind == SEMA_TYPE_COLLECTION &&
        expected->kind == SEMA_TYPE_GENERIC) return true;
    if (actual->kind != expected->kind) return false;
    if (actual_shape != NULL || expected_shape != NULL) {
        if (actual_shape == NULL || expected_shape == NULL ||
            actual_shape->base != expected_shape->base ||
            actual_shape->member_count != expected_shape->member_count) return false;
        for (index = 0U; index < actual_shape->member_count; ++index)
            if (!sema_type_compatible(state, actual_shape->members[index],
                                      expected_shape->members[index]))
                return false;
        return true;
    }
    return actual == expected;
}

static void sema_diag(SemaState *state, const AstNode *node,
                      const char *message, const char *hint) {
    SourceSpan span = node == NULL ? (SourceSpan){ 0U, 0U, 1U, 1U }
                                    : node->span;
    (void)diagnostics_add(state->diagnostics, DIAG_ERROR, DIAG_CODE_TYPE,
                          state->source_name == NULL ? "<source>"
                                                     : state->source_name,
                          span, message, hint);
}

static bool sema_token_text(Token token, const char *text) {
    size_t length = strlen(text);
    return token.lexeme != NULL && token.length == length &&
           memcmp(token.lexeme, text, length) == 0;
}

static bool sema_same_name(Token left, Token right) {
    return left.lexeme != NULL && right.lexeme != NULL &&
           left.length == right.length &&
           memcmp(left.lexeme, right.lexeme, left.length) == 0;
}

static Symbol *sema_find(SemaState *state, Token name) {
    size_t index = state->symbol_count;
    while (index > 0U) {
        Symbol *symbol = &state->symbols[--index];
        if (sema_same_name(symbol->name, name)) return symbol;
    }
    return NULL;
}

static NominalType *sema_find_nominal(SemaState *state, Token name) {
    size_t index;
    for (index = 0U; index < state->nominal_count; ++index)
        if (sema_same_name(state->nominals[index].name, name))
            return &state->nominals[index];
    return NULL;
}

static NominalType *sema_nominal_for_type(SemaState *state,
                                          const SemaType *type) {
    size_t index;
    for (index = 0U; index < state->nominal_count; ++index)
        if (state->nominals[index].type == type) return &state->nominals[index];
    return NULL;
}

static bool sema_is_type_variable(const SemaState *state, Token name) {
    size_t index;
    for (index = 0U; index < state->type_variable_count; ++index)
        if (sema_same_name(state->type_variables[index], name)) return true;
    return false;
}

static size_t sema_generic_count(const AstNode *function) {
    const AstLink *link;
    size_t count = 0U;
    for (link = function->extra.head; link != NULL; link = link->next)
        if (link->node->kind == AST_TYPE_NAME) count += 1U;
    return count;
}

static size_t sema_generic_index(const AstNode *function, Token name) {
    const AstLink *link;
    size_t index = 0U;
    for (link = function->extra.head; link != NULL; link = link->next) {
        if (link->node->kind != AST_TYPE_NAME) continue;
        if (sema_same_name(link->node->token, name)) return index;
        index += 1U;
    }
    return SIZE_MAX;
}

static NominalType *sema_case_owner(SemaState *state, Token case_name,
                                    const SemaType *expected) {
    size_t index;
    for (index = 0U; index < state->nominal_count; ++index) {
        NominalType *nominal = &state->nominals[index];
        const AstLink *link;
        if (expected != NULL && nominal->type != expected) continue;
        for (link = nominal->declaration->items.head; link != NULL;
             link = link->next)
            if (link->node->kind == AST_ENUM_CASE &&
                sema_same_name(link->node->token, case_name)) return nominal;
    }
    return NULL;
}

static const AstNode *sema_case_node(SemaState *state, Token case_name,
                                     const SemaType *expected) {
    NominalType *owner = sema_case_owner(state, case_name, expected);
    const AstLink *link;
    if (owner == NULL) return NULL;
    for (link = owner->declaration->items.head; link != NULL; link = link->next)
        if (link->node->kind == AST_ENUM_CASE &&
            sema_same_name(link->node->token, case_name)) return link->node;
    return NULL;
}

static const char *sema_copy_token(Token token) {
    char *copy;
    if (token.length == SIZE_MAX) return NULL;
    copy = (char *)malloc(token.length + 1U);
    if (copy != NULL) {
        memcpy(copy, token.lexeme, token.length);
        copy[token.length] = '\0';
    }
    return copy;
}

static bool sema_register_nominal(SemaState *state, const AstNode *declaration) {
    SemaTypeKind kind = declaration->kind == AST_TYPE_ALIAS ||
                        declaration->kind == AST_ENUM
                            ? SEMA_TYPE_ADT : SEMA_TYPE_NOMINAL;
    NominalType *existing = sema_find_nominal(state, declaration->token);
    NominalType *entry;
    SemaType *type;
    const char *name;
    if (existing != NULL) {
        sema_diag(state, declaration, "duplicate type declaration",
                  "Choose a unique type name.");
        return true;
    }
    name = sema_copy_token(declaration->token);
    type = (SemaType *)malloc(sizeof(*type));
    if (name == NULL || type == NULL) {
        free((void *)name);
        free(type);
        return false;
    }
    type->kind = kind;
    type->name = name;
    if (!sema_grow((void **)&state->nominals, &state->nominal_capacity,
                   sizeof(*state->nominals), state->nominal_count + 1U)) {
        free((void *)name);
        free(type);
        return false;
    }
    entry = &state->nominals[state->nominal_count++];
    entry->name = declaration->token;
    entry->declaration = declaration;
    entry->type = type;
    return true;
}

static bool sema_pattern_has_case(SemaState *state, const AstNode *pattern,
                                  const SemaType *type, Token name) {
    const AstLink *link;
    if (pattern == NULL) return false;
    if ((pattern->kind == AST_PATTERN_VARIANT ||
         pattern->kind == AST_PATTERN_NAME) &&
        sema_same_name(pattern->token, name) &&
        sema_case_owner(state, name, type) != NULL) return true;
    if (pattern->kind == AST_PATTERN_OR) {
        for (link = pattern->items.head; link != NULL; link = link->next)
            if (sema_pattern_has_case(state, link->node, type, name)) return true;
    }
    return false;
}

static bool sema_declare(SemaState *state, Token name,
                         const AstNode *node, const SemaType *type,
                         bool mutable_binding) {
    size_t index;
    for (index = state->symbol_count; index > 0U; --index) {
        Symbol *existing = &state->symbols[index - 1U];
        if (existing->scope != state->scope) break;
        if (sema_same_name(existing->name, name)) {
            sema_diag(state, node, "duplicate declaration",
                      "Choose a different name or remove the earlier declaration.");
            return false;
        }
    }
    if (!sema_grow((void **)&state->symbols, &state->symbol_capacity,
                   sizeof(*state->symbols), state->symbol_count + 1U)) return false;
    state->symbols[state->symbol_count++] = (Symbol){
        .name = name,
        .declaration = node,
        .value = NULL,
        .type = type,
        .scope = state->scope,
        .mutable_binding = mutable_binding
    };
    return true;
}

static const SemaType *sema_record(SemaState *state, const AstNode *node,
                                   const SemaType *type) {
    size_t index;
    if (node == NULL) return &sema_types[SEMA_TYPE_ERROR];
    for (index = 0U; index < state->type_count; ++index) {
        if (state->types[index].node == node) {
            state->types[index].type = type;
            return type;
        }
    }
    if (!sema_grow((void **)&state->types, &state->type_capacity,
                   sizeof(*state->types), state->type_count + 1U)) return NULL;
    state->types[state->type_count++] =
        (TypeEntry){ node, type, SEMA_ESCAPE_UNKNOWN };
    return type;
}

static void sema_mark_escape(SemaState *state, const AstNode *node,
                             SemaEscape escape) {
    size_t index;
    if (node == NULL) return;
    for (index = 0U; index < state->type_count; ++index) {
        if (state->types[index].node == node) {
            state->types[index].escape = escape;
            return;
        }
    }
}

static void sema_mark_value_escape(SemaState *state, const AstNode *node,
                                   SemaEscape escape) {
    if (node != NULL && node->kind == AST_NAME) {
        Symbol *symbol = sema_find(state, node->token);
        if (symbol != NULL && symbol->value != NULL)
            sema_mark_escape(state, symbol->value, escape);
    }
    sema_mark_escape(state, node, escape);
}

static const SemaType *sema_named_type(SemaState *state, const AstNode *node) {
    Token name = node->token;
    NominalType *nominal;
    if (name.lexeme == NULL) return &sema_types[SEMA_TYPE_UNKNOWN];
    if (sema_token_text(name, "Int") || sema_token_text(name, "Int64"))
        return &sema_types[SEMA_TYPE_INT];
    if (sema_token_text(name, "Float") || sema_token_text(name, "Float64"))
        return &sema_types[SEMA_TYPE_FLOAT];
    if (sema_token_text(name, "Bool")) return &sema_types[SEMA_TYPE_BOOL];
    if (sema_token_text(name, "String")) return &sema_types[SEMA_TYPE_STRING];
    if (sema_token_text(name, "Char")) return &sema_types[SEMA_TYPE_CHAR];
    if (sema_token_text(name, "Unit")) return &sema_types[SEMA_TYPE_UNIT];
    nominal = sema_find_nominal(state, name);
    if (nominal != NULL) return nominal->type;
    sema_diag(state, node, "unknown type name",
              "Use a built-in type or declare this type before use.");
    return &sema_types[SEMA_TYPE_ERROR];
}

static const SemaType *sema_resolve_type(SemaState *state,
                                         const AstNode *type_node) {
    const AstLink *link;
    if (type_node == NULL) return &sema_types[SEMA_TYPE_UNKNOWN];
    if (type_node->kind == AST_TYPE_NAME &&
        sema_is_type_variable(state, type_node->token))
        return &sema_types[SEMA_TYPE_UNKNOWN];
    if (type_node->kind == AST_TYPE_NAME)
        return sema_named_type(state, type_node);
    if (type_node->kind == AST_TYPE_APPLY) {
        Token base_token = type_node->a == NULL ? (Token){ 0 }
                                                : type_node->a->token;
        NominalType *nominal = sema_find_nominal(state, base_token);
        size_t expected_count = SIZE_MAX;
        size_t actual_count = type_node->items.length;
        const SemaType **arguments;
        StrBuf display;
        bool known = true;
        if (sema_token_text(base_token, "List") ||
            sema_token_text(base_token, "Vector") ||
            sema_token_text(base_token, "Matrix") ||
            sema_token_text(base_token, "Set") ||
            sema_token_text(base_token, "Option") ||
            sema_token_text(base_token, "Complex")) expected_count = 1U;
        else if (sema_token_text(base_token, "Map") ||
                 sema_token_text(base_token, "Result") ||
                 sema_token_text(base_token, "Task")) expected_count = 2U;
        else if (sema_token_text(base_token, "Tuple")) expected_count = actual_count;
        else if (nominal != NULL) {
            const AstLink *generic;
            expected_count = 0U;
            for (generic = nominal->declaration->extra.head; generic != NULL;
                 generic = generic->next)
                if (generic->node->kind == AST_TYPE_NAME) expected_count += 1U;
        } else known = false;
        if (!known) {
            sema_diag(state, type_node, "unknown generic type constructor",
                      "Use a declared generic type or a standard collection type.");
            return &sema_types[SEMA_TYPE_ERROR];
        }
        if (actual_count != expected_count) {
            sema_diag(state, type_node, "generic type has the wrong number of arguments",
                      "Provide exactly the type arguments required by this constructor.");
            return &sema_types[SEMA_TYPE_ERROR];
        }
        arguments = actual_count == 0U ? NULL
            : (const SemaType **)malloc(actual_count * sizeof(*arguments));
        if (actual_count > 0U && arguments == NULL)
            return &sema_types[SEMA_TYPE_ERROR];
        str_init(&display);
        if (!str_append(&display, base_token.lexeme, base_token.length) ||
            !str_append_cstr(&display, "[")) {
            free(arguments); str_destroy(&display);
            return &sema_types[SEMA_TYPE_ERROR];
        }
        {
            size_t index = 0U;
            for (link = type_node->items.head; link != NULL; link = link->next) {
                const SemaType *argument = sema_resolve_type(state, link->node);
                arguments[index++] = argument;
                if (index > 1U && !str_append_cstr(&display, ", ")) {
                    free(arguments); str_destroy(&display);
                    return &sema_types[SEMA_TYPE_ERROR];
                }
                if (!str_append_cstr(&display, sema_type_name(argument))) {
                    free(arguments); str_destroy(&display);
                    return &sema_types[SEMA_TYPE_ERROR];
                }
            }
        }
        if (!str_append_cstr(&display, "]")) {
            free(arguments); str_destroy(&display);
            return &sema_types[SEMA_TYPE_ERROR];
        }
        {
            SemaType *applied = sema_new_shape(state, SEMA_TYPE_GENERIC,
                                               str_cstr(&display), arguments,
                                               actual_count,
                                               nominal == NULL ? NULL : nominal->type);
            free(arguments);
            str_destroy(&display);
            return applied == NULL ? &sema_types[SEMA_TYPE_ERROR] : applied;
        }
    }
    if (type_node->kind == AST_TYPE_UNION) {
        const SemaType **members = NULL;
        size_t member_count = 0U;
        size_t member_capacity = 0U;
        StrBuf display;
        for (link = type_node->items.head; link != NULL; link = link->next) {
            const SemaType *member = sema_resolve_type(state, link->node);
            const TypeShape *nested = sema_shape(state, member);
            size_t index;
            if (member->kind == SEMA_TYPE_ERROR) continue;
            if (nested != NULL && member->kind == SEMA_TYPE_UNION) {
                for (index = 0U; index < nested->member_count; ++index) {
                    size_t existing;
                    for (existing = 0U; existing < member_count; ++existing)
                        if (members[existing] == nested->members[index]) break;
                    if (existing == member_count) {
                        if (!sema_grow((void **)&members, &member_capacity,
                                       sizeof(*members), member_count + 1U)) {
                            free(members); return &sema_types[SEMA_TYPE_ERROR];
                        }
                        members[member_count++] = nested->members[index];
                    }
                }
            } else {
                for (index = 0U; index < member_count; ++index)
                    if (members[index] == member) break;
                if (index == member_count) {
                    if (!sema_grow((void **)&members, &member_capacity,
                                   sizeof(*members), member_count + 1U)) {
                        free(members); return &sema_types[SEMA_TYPE_ERROR];
                    }
                    members[member_count++] = member;
                }
            }
        }
        if (member_count == 0U) {
            free(members);
            return &sema_types[SEMA_TYPE_ERROR];
        }
        if (member_count == 1U) {
            const SemaType *only = members[0];
            free(members);
            return only;
        }
        str_init(&display);
        for (link = type_node->items.head; link != NULL; link = link->next) {
            if (display.length > 0U && !str_append_cstr(&display, " | ")) {
                free(members); str_destroy(&display);
                return &sema_types[SEMA_TYPE_ERROR];
            }
            if (!str_append_cstr(&display,
                                 sema_type_name(sema_resolve_type(state,
                                                                  link->node)))) {
                free(members); str_destroy(&display);
                return &sema_types[SEMA_TYPE_ERROR];
            }
        }
        {
            SemaType *union_type = sema_new_shape(state, SEMA_TYPE_UNION,
                                                  str_cstr(&display), members,
                                                  member_count, NULL);
            free(members);
            str_destroy(&display);
            return union_type == NULL ? &sema_types[SEMA_TYPE_ERROR]
                                      : union_type;
        }
    }
    if (type_node->kind == AST_TYPE_FUNCTION)
        sema_diag(state, type_node, "function types are not supported yet",
                  "Use a named function declaration for now.");
    else
        sema_diag(state, type_node, "unsupported type expression",
                  "Use a built-in or declared nominal type.");
    return &sema_types[SEMA_TYPE_ERROR];
}

static const SemaType *sema_expr(SemaState *, const AstNode *);

static bool sema_builtin_name(Token name) {
    return sema_token_text(name, "print") || sema_token_text(name, "sqrt") ||
           sema_token_text(name, "sum") || sema_token_text(name, "typeOf");
}

static void sema_block(SemaState *state, const AstNode *block) {
    const AstLink *link;
    state->scope += 1U;
    for (link = block->items.head; link != NULL; link = link->next)
        (void)sema_expr(state, link->node);
    while (state->symbol_count > 0U &&
           state->symbols[state->symbol_count - 1U].scope == state->scope)
        state->symbol_count -= 1U;
    state->scope -= 1U;
}

static bool sema_bind_pattern(SemaState *state, const AstNode *pattern,
                             const SemaType *type, const AstNode *owner) {
    const AstLink *link;
    if (pattern == NULL) return true;
    switch (pattern->kind) {
        case AST_PATTERN_NAME:
        case AST_PATTERN_SPREAD:
            if (sema_case_owner(state, pattern->token, type) != NULL) return true;
            return sema_declare(state, pattern->token, pattern, type, false);
        case AST_PATTERN_WILDCARD:
        case AST_PATTERN_LITERAL:
        case AST_PATTERN_RANGE:
            return true;
        case AST_PATTERN_TUPLE:
        case AST_PATTERN_LIST:
            if (type->kind != SEMA_TYPE_COLLECTION &&
                type->kind != SEMA_TYPE_UNKNOWN && type->kind != SEMA_TYPE_ERROR)
                sema_diag(state, owner, "destructuring requires a compatible collection",
                          "Match the pattern shape to a tuple, list, or collection value.");
            for (link = pattern->items.head; link != NULL; link = link->next)
                if (!sema_bind_pattern(state, link->node,
                                       &sema_types[SEMA_TYPE_UNKNOWN], owner))
                    return false;
            return true;
        case AST_PATTERN_VARIANT:
            for (link = pattern->items.head; link != NULL; link = link->next)
                if (!sema_bind_pattern(state, link->node,
                                       &sema_types[SEMA_TYPE_UNKNOWN], owner))
                    return false;
            return true;
        case AST_PATTERN_OR:
            sema_diag(state, pattern, "alternative pattern binding is not supported yet",
                      "Use alternatives that do not introduce bindings.");
            return true;
        default:
            sema_diag(state, pattern, "unsupported binding pattern",
                      "Use a name, tuple, or list pattern.");
            return true;
    }
}

static void sema_pop_scope(SemaState *state) {
    while (state->symbol_count > 0U &&
           state->symbols[state->symbol_count - 1U].scope == state->scope)
        state->symbol_count -= 1U;
    state->scope -= 1U;
}

static bool sema_numeric(const SemaType *type) {
    return type->kind == SEMA_TYPE_INT || type->kind == SEMA_TYPE_FLOAT;
}

static const SemaType *sema_function_result(SemaState *state,
                                            const AstNode *function) {
    const SemaType *body_type;
    size_t index;
    if (function->a != NULL) return sema_resolve_type(state, function->a);
    if (function->is_expression_body && function->b != NULL) {
        body_type = NULL;
        for (index = 0U; index < state->type_count; ++index)
            if (state->types[index].node == function->b) {
                body_type = state->types[index].type;
                break;
            }
        return body_type == NULL ? &sema_types[SEMA_TYPE_UNKNOWN] : body_type;
    }
    for (index = 0U; index < state->function_result_count; ++index)
        if (state->function_results[index].function == function)
            return state->function_results[index].result;
    return &sema_types[SEMA_TYPE_UNKNOWN];
}

static const AstNode *sema_parameter_index(const AstNode *function,
                                           size_t target_index) {
    const AstLink *link;
    size_t index = 0U;
    for (link = function->items.head; link != NULL; link = link->next) {
        if (index == target_index) return link->node;
        index += 1U;
    }
    return NULL;
}

static const AstNode *sema_find_parameter_label(const AstNode *function,
                                                Token label,
                                                size_t *parameter_index) {
    const AstLink *link;
    size_t index = 0U;
    for (link = function->items.head; link != NULL; link = link->next, ++index) {
        const AstNode *parameter = link->node;
        Token parameter_label = parameter->token;
        if (parameter->extra.head != NULL)
            parameter_label = parameter->extra.head->node->token;
        if (sema_same_name(parameter_label, label)) {
            *parameter_index = index;
            return parameter;
        }
    }
    return NULL;
}

static const SemaType *sema_check_call(SemaState *state, const AstNode *call,
                                       const AstNode *function) {
    const AstLink *argument_link;
    size_t parameter_count = function->items.length;
    size_t positional_index = 0U;
    size_t index;
    size_t generic_count = sema_generic_count(function);
    bool saw_named = false;
    bool has_variadic = false;
    const SemaType **inferred = generic_count == 0U ? NULL
        : (const SemaType **)calloc(generic_count, sizeof(*inferred));
    bool *used = (bool *)calloc(parameter_count == 0U ? 1U : parameter_count,
                                sizeof(*used));
    if (used == NULL || (generic_count > 0U && inferred == NULL)) {
        free(used);
        free(inferred);
        sema_diag(state, call, "unable to validate function arguments",
                  "Retry compilation with more available memory.");
        return &sema_types[SEMA_TYPE_ERROR];
    }
    for (index = 0U; index < function->items.length; ++index) {
        const AstNode *parameter = sema_parameter_index(function, index);
        if (parameter == NULL) continue;
        if (parameter->flag) has_variadic = true;
    }
    for (argument_link = call->items.head; argument_link != NULL;
         argument_link = argument_link->next) {
        const AstNode *argument = argument_link->node;
        const AstNode *parameter;
        const SemaType *argument_type = sema_expr(state, argument->a);
        sema_mark_value_escape(state, argument->a, SEMA_ESCAPE_ESCAPES);
        size_t parameter_index = positional_index;
        if (argument->token.lexeme != NULL && argument->token.length > 0U) {
            saw_named = true;
            parameter = sema_find_parameter_label(function, argument->token,
                                                  &parameter_index);
            if (parameter == NULL) {
                sema_diag(state, argument, "unknown named argument",
                          "Use a label declared by the called function.");
                continue;
            }
            {
                const AstLink *previous;
                for (previous = call->items.head; previous != argument_link;
                     previous = previous->next)
                    if (previous->node->token.lexeme != NULL &&
                        previous->node->token.length > 0U &&
                        sema_same_name(previous->node->token, argument->token))
                        sema_diag(state, argument, "duplicate named argument",
                                  "Provide each named argument once.");
            }
        } else {
            if (saw_named)
                sema_diag(state, argument,
                          "positional argument follows a named argument",
                          "Place positional arguments before named arguments.");
            parameter_index = positional_index++;
            parameter = sema_parameter_index(function, parameter_index);
            if (parameter == NULL && has_variadic && parameter_count > 0U) {
                const AstNode *last = sema_parameter_index(function,
                                                           parameter_count - 1U);
                if (last != NULL && last->flag) {
                    parameter = last;
                    parameter_index = parameter_count - 1U;
                }
            }
        }
        if (parameter == NULL) {
            if (!has_variadic)
                sema_diag(state, argument, "too many call arguments",
                          "Remove extra arguments or declare a variadic parameter.");
        } else {
            if (used[parameter_index] && !parameter->flag)
                sema_diag(state, argument, "parameter receives more than one argument",
                          "Remove the duplicate positional or named argument.");
            used[parameter_index] = true;
            if (parameter->a != NULL) {
                size_t generic_index = parameter->a->kind == AST_TYPE_NAME
                    ? sema_generic_index(function, parameter->a->token) : SIZE_MAX;
                const SemaType *parameter_type;
                if (generic_index != SIZE_MAX) {
                    if (inferred[generic_index] != NULL &&
                        inferred[generic_index]->kind != argument_type->kind &&
                        argument_type->kind != SEMA_TYPE_UNKNOWN &&
                        argument_type->kind != SEMA_TYPE_ERROR)
                        sema_diag(state, argument,
                                  "generic arguments infer incompatible types",
                                  "Use values of one consistent type for this type parameter.");
                    else if (inferred[generic_index] == NULL)
                        inferred[generic_index] = argument_type;
                }
                parameter_type = generic_index == SIZE_MAX
                    ? sema_resolve_type(state, parameter->a)
                    : &sema_types[SEMA_TYPE_UNKNOWN];
                if (!sema_type_compatible(state, argument_type, parameter_type) &&
                    argument_type->kind != SEMA_TYPE_UNKNOWN &&
                    argument_type->kind != SEMA_TYPE_ERROR &&
                    parameter_type->kind != SEMA_TYPE_UNKNOWN &&
                    parameter_type->kind != SEMA_TYPE_ERROR)
                    sema_diag(state, argument, "argument type does not match parameter",
                              "Pass a value compatible with the parameter type.");
            }
        }
    }
    {
        bool missing_required = false;
        for (index = 0U; index < parameter_count; ++index) {
        const AstNode *parameter = sema_parameter_index(function, index);
        if (parameter != NULL && !used[index] && parameter->b == NULL &&
            !parameter->flag) missing_required = true;
        }
        if (missing_required)
            sema_diag(state, call, "too few call arguments",
                      "Provide values for every required parameter.");
    }
    {
        const SemaType *result;
        size_t result_generic = function->a != NULL &&
                                function->a->kind == AST_TYPE_NAME
            ? sema_generic_index(function, function->a->token) : SIZE_MAX;
        if (result_generic != SIZE_MAX && inferred[result_generic] != NULL)
            result = inferred[result_generic];
        else
            result = sema_function_result(state, function);
        free(used);
        free(inferred);
        return result;
    }
}

static const SemaType *sema_expr(SemaState *state, const AstNode *node) {
    const SemaType *type = &sema_types[SEMA_TYPE_UNIT];
    const SemaType *left;
    const SemaType *right;
    const AstLink *link;
    if (node == NULL) return &sema_types[SEMA_TYPE_ERROR];
    switch (node->kind) {
        case AST_PROGRAM:
        case AST_BLOCK:
            if (node->kind == AST_BLOCK) { sema_block(state, node); break; }
            for (link = node->items.head; link != NULL; link = link->next)
                (void)sema_expr(state, link->node);
            break;
        case AST_BINDING: {
            const AstNode *pattern = node->a;
            const SemaType *declared = node->b == NULL ? NULL
                                                        : sema_resolve_type(state, node->b);
            const SemaType *actual = sema_expr(state, node->c);
            Token name = pattern == NULL ? (Token){ 0 } : pattern->token;
            if (declared != NULL && actual->kind != SEMA_TYPE_ERROR &&
                actual->kind != SEMA_TYPE_UNKNOWN &&
                declared->kind != SEMA_TYPE_ERROR &&
                !sema_type_compatible(state, actual, declared))
                sema_diag(state, node, "initializer type does not match annotation",
                          "Change the annotation or convert the initializer explicitly.");
            type = declared == NULL ? actual : declared;
            if (pattern != NULL && pattern->kind == AST_PATTERN_NAME) {
                if (sema_declare(state, name, pattern, type, node->flag))
                    sema_find(state, name)->value = node->c;
            } else if (pattern != NULL &&
                       !sema_bind_pattern(state, pattern, type, node)) {
                type = &sema_types[SEMA_TYPE_ERROR];
            }
            break;
        }
        case AST_INTEGER:
            type = &sema_types[SEMA_TYPE_INT];
            break;
        case AST_FLOAT:
            type = &sema_types[SEMA_TYPE_FLOAT];
            break;
        case AST_STRING: case AST_INTERPOLATED_STRING:
            type = &sema_types[SEMA_TYPE_STRING];
            break;
        case AST_CHAR:
            type = &sema_types[SEMA_TYPE_CHAR];
            break;
        case AST_BOOLEAN:
            type = &sema_types[SEMA_TYPE_BOOL];
            break;
        case AST_NAME: {
            Symbol *symbol = sema_find(state, node->token);
            if (symbol == NULL) {
                NominalType *nominal = sema_find_nominal(state, node->token);
                NominalType *case_owner = sema_case_owner(state, node->token, NULL);
                if (nominal != NULL)
                    type = nominal->type;
                else if (case_owner != NULL)
                    type = case_owner->type;
                else if (sema_builtin_name(node->token))
                    type = &sema_types[SEMA_TYPE_FUNCTION];
                else {
                    sema_diag(state, node, "unknown name",
                              "Declare the name in this scope or import its module.");
                    type = &sema_types[SEMA_TYPE_ERROR];
                }
            } else type = symbol->type;
            break;
        }
        case AST_TYPE_NAME: type = sema_named_type(state, node); break;
        case AST_UNARY:
            left = sema_expr(state, node->a);
            if (node->op == TOK_NOT) {
                if (left->kind != SEMA_TYPE_BOOL && left->kind != SEMA_TYPE_ERROR)
                    sema_diag(state, node, "'not' requires Bool",
                              "Use a Bool expression for logical negation.");
                type = &sema_types[SEMA_TYPE_BOOL];
            } else type = left;
            break;
        case AST_BINARY: case AST_ASSIGNMENT:
            left = sema_expr(state, node->a);
            right = sema_expr(state, node->b);
            if (node->kind == AST_ASSIGNMENT) {
                if (node->a != NULL && node->a->kind == AST_NAME) {
                    Symbol *symbol = sema_find(state, node->a->token);
                    if (symbol != NULL && !symbol->mutable_binding)
                        sema_diag(state, node->a, "cannot assign to immutable binding",
                                  "Declare this binding with 'var' to allow reassignment.");
                }
                if (!sema_type_compatible(state, right, left) &&
                    left->kind != SEMA_TYPE_ERROR &&
                    right->kind != SEMA_TYPE_ERROR &&
                    left->kind != SEMA_TYPE_UNKNOWN &&
                    right->kind != SEMA_TYPE_UNKNOWN)
                    sema_diag(state, node, "assigned value has incompatible type",
                              "Use a value with the binding's declared type.");
                type = left;
            } else if (node->op == TOK_AND || node->op == TOK_OR ||
                       node->op == TOK_EQUAL_EQUAL || node->op == TOK_BANG_EQUAL ||
                       node->op == TOK_LESS || node->op == TOK_LESS_EQUAL ||
                       node->op == TOK_GREATER || node->op == TOK_GREATER_EQUAL) {
                if ((node->op == TOK_AND || node->op == TOK_OR) &&
                    (left->kind != SEMA_TYPE_BOOL || right->kind != SEMA_TYPE_BOOL))
                    sema_diag(state, node, "logical operators require Bool operands",
                              "Use Bool values on both sides.");
                if (node->op != TOK_AND && node->op != TOK_OR &&
                    left->kind != right->kind &&
                    left->kind != SEMA_TYPE_ERROR &&
                    right->kind != SEMA_TYPE_ERROR &&
                    left->kind != SEMA_TYPE_UNKNOWN &&
                    right->kind != SEMA_TYPE_UNKNOWN)
                    sema_diag(state, node, "comparison operands have incompatible types",
                              "Compare values of compatible types.");
                type = &sema_types[SEMA_TYPE_BOOL];
            } else {
                if (!sema_type_compatible(state, left, right) && left->kind != SEMA_TYPE_ERROR &&
                    right->kind != SEMA_TYPE_ERROR)
                    sema_diag(state, node, "operator operands have incompatible types",
                              "Use matching operand types or an explicit conversion.");
                if (!sema_numeric(left) && left->kind != SEMA_TYPE_ERROR &&
                    left->kind != SEMA_TYPE_UNKNOWN)
                    sema_diag(state, node, "arithmetic operator requires numeric operands",
                              "Use Int64 or Float64 values for arithmetic.");
                type = left;
            }
            break;
        case AST_CALL:
            left = sema_expr(state, node->a);
            if (node->a != NULL && node->a->kind == AST_NAME &&
                (left->kind == SEMA_TYPE_NOMINAL || left->kind == SEMA_TYPE_ADT)) {
                NominalType *nominal = sema_nominal_for_type(state, left);
                if (nominal != NULL && nominal->declaration->kind == AST_CLASS) {
                    const AstLink *member;
                    const AstNode *initializer = NULL;
                    for (member = nominal->declaration->items.head; member != NULL;
                         member = member->next)
                        if (member->node->kind == AST_FUNCTION &&
                            member->node->is_constructor) {
                            initializer = member->node;
                            break;
                        }
                    if (initializer != NULL)
                        (void)sema_check_call(state, node, initializer);
                    else if (node->items.length != 0U)
                        sema_diag(state, node, "class constructor takes no arguments",
                                  "Declare an init method accepting these arguments.");
                    type = left;
                    break;
                }
            }
            if (node->a != NULL && node->a->kind == AST_MEMBER) {
                const AstNode *member_node = node->a;
                const SemaType *receiver_type = sema_expr(state, member_node->a);
                NominalType *nominal = sema_nominal_for_type(state, receiver_type);
                const AstLink *member;
                if (nominal != NULL && nominal->declaration->kind == AST_CLASS) {
                    for (member = nominal->declaration->items.head; member != NULL;
                         member = member->next) {
                        const AstNode *method = member->node;
                        if (method->kind == AST_FUNCTION && method->is_method &&
                            sema_same_name(method->token, member_node->token)) {
                            type = sema_check_call(state, node, method);
                            break;
                        }
                    }
                    if (member != NULL) break;
                }
            }
            if (node->a != NULL && node->a->kind == AST_NAME) {
                Symbol *callee = sema_find(state, node->a->token);
                if (callee != NULL && callee->declaration != NULL &&
                    callee->declaration->kind == AST_FUNCTION) {
                    type = sema_check_call(state, node, callee->declaration);
                    break;
                }
            }
            for (link = node->items.head; link != NULL; link = link->next) {
                (void)sema_expr(state, link->node->a);
                sema_mark_value_escape(state, link->node->a,
                                       SEMA_ESCAPE_ESCAPES);
            }
            type = left->kind == SEMA_TYPE_ADT || left->kind == SEMA_TYPE_NOMINAL
                       ? left : &sema_types[SEMA_TYPE_UNKNOWN];
            break;
        case AST_MEMBER: {
            NominalType *nominal;
            const AstLink *member;
            bool found = false;
            left = sema_expr(state, node->a);
            nominal = sema_nominal_for_type(state, left);
            if (nominal == NULL || nominal->declaration->kind != AST_CLASS) {
                type = &sema_types[SEMA_TYPE_UNKNOWN];
                break;
            }
            for (member = nominal->declaration->items.head; member != NULL;
                 member = member->next) {
                const AstNode *declaration = member->node;
                if (declaration->kind == AST_FUNCTION &&
                    declaration->is_constructor) {
                    const AstLink *parameter;
                    for (parameter = declaration->items.head; parameter != NULL;
                         parameter = parameter->next) {
                        const AstNode *field = parameter->node;
                        if (field->flag && sema_same_name(field->token,
                                                          node->token)) {
                            found = true;
                            type = sema_resolve_type(state, field->a);
                            if (state->current_class != nominal)
                                sema_diag(state, node, "class member is not accessible",
                                          "Access this member from its class or make it public.");
                            break;
                        }
                    }
                    if (found) break;
                }
                if (!sema_same_name(declaration->token, node->token)) continue;
                found = true;
                if (state->current_class != nominal &&
                    !(declaration->kind == AST_BINDING
                          ? declaration->second_flag : declaration->flag))
                    sema_diag(state, node, "class member is not accessible",
                              "Access this member from its class or make it public.");
                if (declaration->kind == AST_BINDING)
                    type = sema_resolve_type(state, declaration->a);
                else if (declaration->kind == AST_FUNCTION)
                    type = &sema_types[SEMA_TYPE_FUNCTION];
                else
                    type = &sema_types[SEMA_TYPE_UNKNOWN];
                break;
            }
            if (!found) {
                sema_diag(state, node, "unknown class member",
                          "Use a field or method declared by this class.");
                type = &sema_types[SEMA_TYPE_ERROR];
            }
            break;
        }
        case AST_LIST: case AST_TUPLE: {
            if (node->kind == AST_TUPLE && node->items.length == 0U) {
                type = &sema_types[SEMA_TYPE_UNIT];
                break;
            }
            if (node->kind == AST_LIST && node->flag) {
                (void)sema_expr(state, node->b);
                state->scope += 1U;
                (void)sema_bind_pattern(state, node->a,
                                        &sema_types[SEMA_TYPE_UNKNOWN], node);
                if (node->items.head != NULL)
                    (void)sema_expr(state, node->items.head->node->a);
                sema_pop_scope(state);
                type = &sema_types[SEMA_TYPE_COLLECTION];
                break;
            }
            const SemaType *element_type = NULL;
            for (link = node->items.head; link != NULL; link = link->next)
            {
                const AstNode *item = link->node->kind == AST_ARGUMENT
                                          ? link->node->a : link->node;
                const SemaType *item_type = sema_expr(state, item);
                if (node->kind == AST_LIST && element_type == NULL)
                    element_type = item_type;
                else if (node->kind == AST_LIST &&
                         item_type->kind != element_type->kind &&
                         item_type->kind != SEMA_TYPE_ERROR &&
                         element_type->kind != SEMA_TYPE_ERROR)
                    sema_diag(state, link->node, "list elements have incompatible types",
                              "Use elements with one consistent inferred type.");
            }
            type = &sema_types[SEMA_TYPE_COLLECTION];
            break;
        }
        case AST_INDEX:
            left = sema_expr(state, node->a);
            for (link = node->items.head; link != NULL; link = link->next) {
                right = sema_expr(state, link->node);
                if (right->kind != SEMA_TYPE_INT &&
                    right->kind != SEMA_TYPE_ERROR &&
                    right->kind != SEMA_TYPE_UNKNOWN)
                    sema_diag(state, link->node, "index must be Int64",
                              "Use an integer index.");
            }
            if (left->kind == SEMA_TYPE_STRING) type = &sema_types[SEMA_TYPE_CHAR];
            else if (left->kind == SEMA_TYPE_COLLECTION ||
                     left->kind == SEMA_TYPE_GENERIC ||
                     left->kind == SEMA_TYPE_ERROR ||
                     left->kind == SEMA_TYPE_UNKNOWN)
                type = &sema_types[SEMA_TYPE_UNKNOWN];
            else {
                sema_diag(state, node, "value is not indexable",
                          "Index a string, tuple, or collection.");
                type = &sema_types[SEMA_TYPE_ERROR];
            }
            break;
        case AST_RANGE:
            left = sema_expr(state, node->a);
            right = sema_expr(state, node->b);
            if (!sema_numeric(left) || !sema_type_compatible(state, left, right)) {
                if (left->kind != SEMA_TYPE_ERROR &&
                    right->kind != SEMA_TYPE_ERROR)
                    sema_diag(state, node, "range endpoints must have one numeric type",
                              "Use matching Int64 or Float64 endpoints.");
            }
            type = &sema_types[SEMA_TYPE_COLLECTION];
            break;
        case AST_LAMBDA: {
            state->scope += 1U;
            for (link = node->items.head; link != NULL; link = link->next) {
                const AstNode *parameter = link->node;
                const SemaType *parameter_type = parameter->a == NULL
                    ? &sema_types[SEMA_TYPE_UNKNOWN]
                    : sema_resolve_type(state, parameter->a);
                (void)sema_declare(state, parameter->token, parameter,
                                   parameter_type, false);
            }
            (void)sema_expr(state, node->a);
            sema_pop_scope(state);
            type = &sema_types[SEMA_TYPE_FUNCTION];
            break;
        }
        case AST_KERNEL: {
            (void)sema_expr(state, node->a);
            state->scope += 1U;
            (void)sema_declare(state, node->token, node,
                               &sema_types[SEMA_TYPE_UNKNOWN], false);
            (void)sema_expr(state, node->b);
            sema_pop_scope(state);
            type = &sema_types[SEMA_TYPE_COLLECTION];
            break;
        }
        case AST_SPAWN: case AST_AWAIT: case AST_TRY: case AST_PARALLEL:
            type = sema_expr(state, node->a);
            break;
        case AST_EXPRESSION_STATEMENT:
            type = sema_expr(state, node->a); break;
        case AST_IF:
            left = sema_expr(state, node->a);
            if (left->kind != SEMA_TYPE_BOOL && left->kind != SEMA_TYPE_ERROR)
                sema_diag(state, node->a, "if condition must be Bool",
                          "Compare values explicitly; conditions have no truthiness.");
            sema_block(state, node->b);
            if (node->c != NULL) (void)sema_expr(state, node->c);
            break;
        case AST_WHILE:
            left = sema_expr(state, node->a);
            if (left->kind != SEMA_TYPE_BOOL && left->kind != SEMA_TYPE_ERROR)
                sema_diag(state, node->a, "while condition must be Bool",
                          "Use a Bool condition.");
            state->loop_depth += 1U; sema_block(state, node->b);
            state->loop_depth -= 1U; break;
        case AST_FOR:
            (void)sema_expr(state, node->b);
            state->scope += 1U;
            (void)sema_bind_pattern(state, node->a,
                                    &sema_types[SEMA_TYPE_UNKNOWN], node);
            state->loop_depth += 1U;
            sema_block(state, node->c);
            state->loop_depth -= 1U;
            sema_pop_scope(state);
            break;
        case AST_RETURN:
            if (state->function_depth == 0U)
                sema_diag(state, node, "return is only valid inside a function",
                          "Move this statement into a function.");
            left = node->a == NULL ? &sema_types[SEMA_TYPE_UNIT]
                                   : sema_expr(state, node->a);
            sema_mark_value_escape(state, node->a, SEMA_ESCAPE_ESCAPES);
            if (state->return_type != NULL &&
                state->return_type->kind == SEMA_TYPE_UNKNOWN &&
                left->kind != SEMA_TYPE_ERROR &&
                left->kind != SEMA_TYPE_UNKNOWN) {
                if (state->inferred_return_type == NULL)
                    state->inferred_return_type = left;
                else if (!sema_type_compatible(state, left,
                                               state->inferred_return_type))
                    sema_diag(state, node,
                              "function returns incompatible value types",
                              "Return values with one consistent inferred type.");
            }
            if (state->return_type != NULL &&
                state->return_type->kind != SEMA_TYPE_UNKNOWN &&
                !sema_type_compatible(state, left, state->return_type) &&
                left->kind != SEMA_TYPE_ERROR)
                sema_diag(state, node, "return type does not match function result",
                          "Return a value of the declared result type.");
            break;
        case AST_BREAK: case AST_CONTINUE:
            if (state->loop_depth == 0U)
                sema_diag(state, node, "loop control used outside a loop",
                          "Place break/continue inside a loop.");
            break;
        case AST_MATCH: {
            bool has_wildcard = false;
            bool has_true = false;
            bool has_false = false;
            NominalType *matched_nominal = NULL;
            bool *seen_cases = NULL;
            size_t case_count = 0U;
            left = sema_expr(state, node->a);
            if (left->kind == SEMA_TYPE_ADT) {
                size_t nominal_index;
                for (nominal_index = 0U; nominal_index < state->nominal_count;
                     ++nominal_index) {
                    NominalType *candidate = &state->nominals[nominal_index];
                    if (candidate->type == left) {
                        const AstLink *case_link;
                        matched_nominal = candidate;
                        for (case_link = candidate->declaration->items.head;
                             case_link != NULL; case_link = case_link->next)
                            if (case_link->node->kind == AST_ENUM_CASE)
                                case_count += 1U;
                        break;
                    }
                }
                if (case_count > 0U) {
                    seen_cases = (bool *)calloc(case_count, sizeof(*seen_cases));
                    if (seen_cases == NULL) {
                        type = &sema_types[SEMA_TYPE_ERROR];
                        break;
                    }
                }
            }
            for (link = node->items.head; link != NULL; link = link->next) {
                const AstNode *arm = link->node;
                const AstNode *pattern = arm->a;
                const SemaType *arm_type;
                bool guarded = arm->b != NULL;
                if (pattern != NULL &&
                    (pattern->kind == AST_PATTERN_WILDCARD ||
                     (pattern->kind == AST_PATTERN_NAME &&
                      sema_case_owner(state, pattern->token, left) == NULL))) {
                    if (has_wildcard)
                        sema_diag(state, arm, "unreachable match arm",
                                  "A previous unguarded wildcard or binding covers every value.");
                    if (!guarded) has_wildcard = true;
                }
                if (pattern != NULL && pattern->kind == AST_PATTERN_LITERAL &&
                    (pattern->token.kind == TOK_TRUE ||
                     pattern->token.kind == TOK_FALSE)) {
                    bool *seen = pattern->token.kind == TOK_TRUE ? &has_true : &has_false;
                    if (*seen)
                        sema_diag(state, arm, "duplicate Bool match case",
                                  "Remove the duplicate case or add a guard.");
                    if (!guarded) *seen = true;
                }
                if (matched_nominal != NULL && pattern != NULL) {
                    const AstLink *case_link;
                    size_t case_index = 0U;
                    for (case_link = matched_nominal->declaration->items.head;
                         case_link != NULL; case_link = case_link->next) {
                        if (case_link->node->kind != AST_ENUM_CASE) continue;
                        if (sema_pattern_has_case(state, pattern, left,
                                                 case_link->node->token)) {
                            if (seen_cases[case_index] && !guarded)
                                sema_diag(state, arm, "duplicate ADT match case",
                                          "Remove the duplicate variant arm.");
                            if (!guarded) seen_cases[case_index] = true;
                        }
                        {
                            const AstLink *alternative;
                            for (alternative = arm->items.head; alternative != NULL;
                                 alternative = alternative->next)
                                if (!guarded &&
                                    sema_pattern_has_case(state, alternative->node,
                                                         left, case_link->node->token))
                                    seen_cases[case_index] = true;
                        }
                        case_index += 1U;
                    }
                    if (pattern->kind == AST_PATTERN_VARIANT ||
                        pattern->kind == AST_PATTERN_NAME) {
                        const AstNode *case_node = sema_case_node(state,
                                                                 pattern->token,
                                                                 left);
                        if (case_node == NULL) {
                            if (pattern->kind == AST_PATTERN_VARIANT)
                                sema_diag(state, pattern,
                                          "unknown variant in match pattern",
                                          "Use a variant declared by the matched ADT.");
                        } else if (case_node->items.length !=
                                 (pattern->kind == AST_PATTERN_VARIANT
                                      ? pattern->items.length : 0U))
                            sema_diag(state, pattern, "variant pattern has wrong payload arity",
                                      "Provide one pattern for each variant payload.");
                    }
                }
                if (guarded) {
                    const SemaType *guard_type = sema_expr(state, arm->b);
                    if (guard_type->kind != SEMA_TYPE_BOOL &&
                        guard_type->kind != SEMA_TYPE_ERROR)
                        sema_diag(state, arm->b, "match guard must be Bool",
                                  "Use a Bool expression after 'if'.");
                }
                state->scope += 1U;
                (void)sema_bind_pattern(state, pattern, left, arm);
                arm_type = sema_expr(state, arm->c);
                if (type == &sema_types[SEMA_TYPE_UNIT]) type = arm_type;
                else if (!sema_type_compatible(state, type, arm_type) &&
                         arm_type->kind != SEMA_TYPE_ERROR &&
                         type->kind != SEMA_TYPE_ERROR &&
                         arm_type->kind != SEMA_TYPE_UNKNOWN &&
                         type->kind != SEMA_TYPE_UNKNOWN)
                    sema_diag(state, arm, "match arms have incompatible result types",
                              "Return compatible values from every arm.");
                sema_pop_scope(state);
            }
            if (!has_wildcard && matched_nominal != NULL) {
                size_t case_index;
                for (case_index = 0U; case_index < case_count; ++case_index)
                    if (!seen_cases[case_index]) {
                        sema_diag(state, node, "match expression is not exhaustive",
                                  "Add an arm for every ADT variant or a wildcard.");
                        break;
                    }
            } else if (!has_wildcard) {
                bool exhaustive = false;
                if (left->kind == SEMA_TYPE_BOOL) {
                    exhaustive = has_true && has_false;
                } else if (left->kind == SEMA_TYPE_UNION) {
                    const TypeShape *union_shape = sema_shape(state, left);
                    size_t member_index;
                    exhaustive = union_shape != NULL;
                    for (member_index = 0U;
                         exhaustive && member_index < union_shape->member_count;
                         ++member_index) {
                        const SemaType *member = union_shape->members[member_index];
                        if (member->kind == SEMA_TYPE_BOOL) {
                            if (!has_true || !has_false) exhaustive = false;
                        } else {
                            exhaustive = false;
                        }
                    }
                }
                if (!exhaustive)
                    sema_diag(state, node, "match expression is not exhaustive",
                              "Add a wildcard arm or cover every finite value of the matched type.");
            }
            free(seen_cases);
            break;
        }
        case AST_FUNCTION: {
            const SemaType *previous_return = state->return_type;
            const SemaType *previous_inferred_return = state->inferred_return_type;
            const SemaType *result;
            size_t previous_type_variables = state->type_variable_count;
            state->scope += 1U;
            for (link = node->extra.head; link != NULL; link = link->next) {
                if (link->node->kind == AST_TYPE_NAME) {
                    if (!sema_grow((void **)&state->type_variables,
                                   &state->type_variable_capacity,
                                   sizeof(*state->type_variables),
                                   state->type_variable_count + 1U)) {
                        result = &sema_types[SEMA_TYPE_ERROR];
                        break;
                    }
                    state->type_variables[state->type_variable_count++] =
                        link->node->token;
                }
            }
            result = node->a == NULL ? &sema_types[SEMA_TYPE_UNKNOWN]
                                     : sema_resolve_type(state, node->a);
            for (link = node->items.head; link != NULL; link = link->next) {
                AstNode *parameter = link->node;
                const SemaType *parameter_type = parameter->a == NULL
                    ? &sema_types[SEMA_TYPE_UNKNOWN]
                    : sema_resolve_type(state, parameter->a);
                if (parameter->token.lexeme != NULL)
                    (void)sema_declare(state, parameter->token, parameter,
                                       parameter_type, false);
            }
            state->return_type = result;
            state->inferred_return_type = NULL;
            state->function_depth += 1U;
            if (node->b != NULL && node->b->kind == AST_BLOCK) sema_block(state, node->b);
            else {
                const SemaType *body_type = sema_expr(state, node->b);
                if (node->a != NULL && !sema_type_compatible(state, body_type, result) &&
                    body_type->kind != SEMA_TYPE_ERROR &&
                    result->kind != SEMA_TYPE_ERROR &&
                    body_type->kind != SEMA_TYPE_UNKNOWN)
                    sema_diag(state, node, "function body type does not match result annotation",
                              "Return or infer a value matching the declared result type.");
            }
            if (node->a == NULL && node->b != NULL &&
                node->b->kind == AST_BLOCK &&
                state->inferred_return_type != NULL) {
                if (sema_grow((void **)&state->function_results,
                              &state->function_result_capacity,
                              sizeof(*state->function_results),
                              state->function_result_count + 1U))
                    state->function_results[state->function_result_count++] =
                        (FunctionResult){ node, state->inferred_return_type };
                else
                    type = &sema_types[SEMA_TYPE_ERROR];
            }
            state->function_depth -= 1U;
            state->return_type = previous_return;
            state->inferred_return_type = previous_inferred_return;
            state->type_variable_count = previous_type_variables;
            while (state->symbol_count > 0U &&
                   state->symbols[state->symbol_count - 1U].scope == state->scope)
                state->symbol_count -= 1U;
            state->scope -= 1U;
            break;
        }
        case AST_CLASS: {
            const AstLink *member;
            NominalType *nominal = sema_find_nominal(state, node->token);
            NominalType *previous_class = state->current_class;
            const SemaType *base_type = NULL;
            Token receiver = { TOK_SELF, node->span, "self", 4U };
            state->current_class = nominal;
            if (node->a != NULL) {
                base_type = sema_resolve_type(state, node->a);
                {
                    NominalType *interface_type = sema_nominal_for_type(state,
                                                                         base_type);
                    if (interface_type != NULL &&
                        interface_type->declaration->kind == AST_INTERFACE) {
                        const AstLink *signature;
                        for (signature = interface_type->declaration->items.head;
                             signature != NULL; signature = signature->next) {
                            const AstNode *required = signature->node;
                            const AstNode *implemented = NULL;
                            for (member = node->items.head; member != NULL;
                                 member = member->next)
                                if (member->node->kind == AST_FUNCTION &&
                                    member->node->is_method &&
                                    sema_same_name(member->node->token,
                                                   required->token)) {
                                    implemented = member->node;
                                    break;
                                }
                            if (implemented == NULL) {
                                sema_diag(state, node,
                                          "class does not implement interface method",
                                          "Add every method required by the interface.");
                                continue;
                            }
                            if (implemented->items.length != required->items.length)
                                sema_diag(state, implemented,
                                          "interface method parameter count does not match",
                                          "Match the interface method signature.");
                            {
                                const AstLink *required_parameter =
                                    required->items.head;
                                const AstLink *implemented_parameter =
                                    implemented->items.head;
                                while (required_parameter != NULL &&
                                       implemented_parameter != NULL) {
                                    const SemaType *required_type =
                                        sema_resolve_type(state,
                                            required_parameter->node->a);
                                    const SemaType *implemented_type =
                                        sema_resolve_type(state,
                                            implemented_parameter->node->a);
                                    if (!sema_same_name(
                                            required_parameter->node->token,
                                            implemented_parameter->node->token) ||
                                        (required_parameter->node->a == NULL) !=
                                            (implemented_parameter->node->a == NULL) ||
                                        !sema_type_compatible(state, required_type,
                                                              implemented_type) ||
                                        !sema_type_compatible(state,
                                            implemented_type, required_type))
                                        sema_diag(state, implemented_parameter->node,
                                                  "interface parameter type does not match",
                                                  "Use the parameter type declared by the interface.");
                                    required_parameter = required_parameter->next;
                                    implemented_parameter =
                                        implemented_parameter->next;
                                }
                            }
                            if ((required->a == NULL) !=
                                (implemented->a == NULL)) {
                                sema_diag(state, implemented,
                                          "interface result type does not match",
                                          "Use the result type declared by the interface.");
                            } else if (required->a != NULL) {
                                const SemaType *required_result =
                                    sema_resolve_type(state, required->a);
                                const SemaType *implemented_result =
                                    sema_resolve_type(state, implemented->a);
                                if (!sema_type_compatible(state, required_result,
                                                          implemented_result) ||
                                    !sema_type_compatible(state, implemented_result,
                                                          required_result))
                                    sema_diag(state, implemented,
                                              "interface result type does not match",
                                              "Use the result type declared by the interface.");
                            }
                        }
                    }
                }
            }
            for (member = node->items.head; member != NULL; member = member->next) {
                const AstNode *member_node = member->node;
                if (member_node->kind == AST_BINDING) {
                    const SemaType *declared = sema_resolve_type(state,
                                                                  member_node->a);
                    if (member_node->c != NULL) {
                        const SemaType *actual = sema_expr(state, member_node->c);
                        if (!sema_type_compatible(state, actual, declared) &&
                            actual->kind != SEMA_TYPE_ERROR &&
                            declared->kind != SEMA_TYPE_ERROR)
                            sema_diag(state, member_node,
                                      "field initializer type does not match annotation",
                                      "Initialize the field with a compatible value.");
                    }
                } else if (member_node->kind == AST_FUNCTION) {
                    state->scope += 1U;
                    (void)sema_declare(state, receiver, member_node,
                                       nominal == NULL ?
                                           &sema_types[SEMA_TYPE_UNKNOWN] : nominal->type,
                                       false);
                    (void)sema_expr(state, member_node);
                    sema_pop_scope(state);
                }
            }
            state->current_class = previous_class;
            break;
        }
        case AST_INTERFACE: {
            const AstLink *member;
            for (member = node->items.head; member != NULL; member = member->next)
                (void)sema_expr(state, member->node);
            break;
        }
        case AST_ENUM: case AST_TYPE_ALIAS: {
            const AstLink *item;
            for (item = node->items.head; item != NULL; item = item->next) {
                const AstLink *payload;
                for (payload = item->node->items.head; payload != NULL;
                     payload = payload->next)
                    (void)sema_resolve_type(state, payload->node);
                if (item->node->kind == AST_FUNCTION)
                    (void)sema_expr(state, item->node);
            }
            break;
        }
        case AST_IMPORT:
            sema_diag(state, node, "module imports are not resolved yet",
                      "Only names declared in this source module are currently available.");
            break;
        default:
            if (node->a != NULL) (void)sema_expr(state, node->a);
            if (node->b != NULL) (void)sema_expr(state, node->b);
            if (node->c != NULL) (void)sema_expr(state, node->c);
            for (link = node->items.head; link != NULL; link = link->next)
                (void)sema_expr(state, link->node);
            type = &sema_types[SEMA_TYPE_UNKNOWN];
            break;
    }
    {
        const SemaType *recorded = sema_record(state, node, type);
        if (recorded == NULL) return &sema_types[SEMA_TYPE_ERROR];
        if (recorded != NULL &&
            (node->kind == AST_INTEGER || node->kind == AST_FLOAT ||
             node->kind == AST_STRING || node->kind == AST_INTERPOLATED_STRING ||
             node->kind == AST_CHAR || node->kind == AST_BOOLEAN))
            sema_mark_escape(state, node, SEMA_ESCAPE_NON_ESCAPING);
        return recorded;
    }
}

void sema_context_init(SemaContext *context) {
    if (context != NULL) context->implementation = calloc(1U, sizeof(SemaState));
}

void sema_context_destroy(SemaContext *context) {
    size_t index;
    SemaState *state;
    if (context == NULL || context->implementation == NULL) return;
    state = (SemaState *)context->implementation;
    for (index = 0U; index < state->nominal_count; ++index) {
        free((void *)state->nominals[index].type->name);
        free(state->nominals[index].type);
    }
    for (index = 0U; index < state->shape_count; ++index) {
        free((void *)state->shapes[index].type->name);
        free(state->shapes[index].type);
        free(state->shapes[index].members);
    }
    free(state->shapes);
    free(state->function_results);
    free(state->nominals);
    free(state->type_variables);
    free(state->types); free(state->symbols); free(state);
    context->implementation = NULL;
}

bool sema_analyze(const AstNode *program, SemaContext *context,
                  Diagnostics *diagnostics) {
    SemaState *state;
    if (program == NULL || context == NULL || diagnostics == NULL) return false;
    if (context->implementation == NULL) sema_context_init(context);
    if (context->implementation == NULL) return false;
    state = (SemaState *)context->implementation;
    free(state->types); free(state->symbols);
    free(state->function_results);
    {
        size_t index;
        for (index = 0U; index < state->shape_count; ++index) {
            free((void *)state->shapes[index].type->name);
            free(state->shapes[index].type);
            free(state->shapes[index].members);
        }
        free(state->shapes);
    }
    {
        size_t index;
        for (index = 0U; index < state->nominal_count; ++index) {
            free((void *)state->nominals[index].type->name);
            free(state->nominals[index].type);
        }
        free(state->nominals);
    }
    free(state->type_variables);
    memset(state, 0, sizeof(*state));
    state->diagnostics = diagnostics;
    state->source_name = program->token.lexeme;
    if (program->kind == AST_PROGRAM) {
        const AstLink *link;
        for (link = program->items.head; link != NULL; link = link->next) {
            const AstNode *declaration = link->node;
            if (declaration->kind == AST_CLASS ||
                declaration->kind == AST_INTERFACE ||
                declaration->kind == AST_ENUM ||
                declaration->kind == AST_TYPE_ALIAS) {
                if (!sema_register_nominal(state, declaration)) return false;
            }
        }
        for (link = program->items.head; link != NULL; link = link->next) {
            const AstNode *declaration = link->node;
            if (declaration->kind == AST_FUNCTION &&
                !sema_declare(state, declaration->token, declaration,
                              &sema_types[SEMA_TYPE_FUNCTION], false))
                continue;
        }
    }
    (void)sema_expr(state, program);
    return true;
}

const SemaType *sema_type_of(const SemaContext *context, const AstNode *node) {
    const SemaState *state;
    size_t index;
    if (context == NULL || node == NULL || context->implementation == NULL) return NULL;
    state = (const SemaState *)context->implementation;
    for (index = 0U; index < state->type_count; ++index)
        if (state->types[index].node == node) return state->types[index].type;
    return NULL;
}

const char *sema_type_name(const SemaType *type) {
    return type == NULL || type->name == NULL ? "<unknown>" : type->name;
}

SemaEscape sema_escape_of(const SemaContext *context, const AstNode *node) {
    const SemaState *state;
    size_t index;
    if (context == NULL || node == NULL || context->implementation == NULL)
        return SEMA_ESCAPE_UNKNOWN;
    state = (const SemaState *)context->implementation;
    for (index = 0U; index < state->type_count; ++index)
        if (state->types[index].node == node) return state->types[index].escape;
    return SEMA_ESCAPE_UNKNOWN;
}
