#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "parser.h"

namespace glsl {

parser::parser(const std::string &source)
    : m_lexer(source)
{
}

parser::~parser() {
    // TODO: reference count m_ast
}

#define IS_TYPE(TOKEN, TYPE) \
    ((TOKEN).m_type == (TYPE))
#define IS_KEYWORD(TOKEN, KEYWORD) \
    (IS_TYPE((TOKEN), kType_keyword) && (TOKEN).m_keyword == (KEYWORD))
#define IS_OPERATOR(TOKEN, OPERATOR) \
    (IS_TYPE((TOKEN), kType_operator) && (TOKEN).m_operator == (OPERATOR))

#define GC_NEW(X) new(gc<X>())

bool parser::isType(int type) const {
    return IS_TYPE(m_token, type);
}

bool parser::isKeyword(int keyword) const {
    return IS_KEYWORD(m_token, keyword);
}

bool parser::isOperator(int oper) const {
    return IS_OPERATOR(m_token, oper);
}

bool parser::isEndCondition(endCondition condition) const {
    return ((condition & kEndConditionSemicolon)    && isType(kType_semicolon))
        || ((condition & kEndConditionParanthesis)  && isOperator(kOperator_paranthesis_end))
        || ((condition & kEndConditionBracket)      && isOperator(kOperator_bracket_end))
        || ((condition & kEndConditionColon)        && isOperator(kOperator_colon))
        || ((condition & kEndConditionComma)        && isOperator(kOperator_comma));
}

void parser::fatal(const char *fmt, ...) {
    char buffer[1024];
    snprintf(buffer, sizeof(buffer), "<string>:%zu:%zu: error: ", m_lexer.line(), m_lexer.column());
    m_error = buffer;
    va_list va;
    va_start(va, fmt);
    snprintf(buffer, sizeof(buffer), fmt, va);
    va_end(va);
    m_error += buffer;
    longjmp(m_exit, 1);
}

#undef TYPENAME
#define TYPENAME(X) case kKeyword_##X:
bool parser::isBuiltin() const {
    if (!isType(kType_keyword))
        return false;
    switch (m_token.m_keyword) {
        #include "lexemes.h"
            return true;
        default:
            break;
    }
    return false;
}
#undef TYPENAME
#define TYPENAME(...)

/// The parser entry point
astTU *parser::parse() {
    if (setjmp(m_exit)) {
        return 0;
    } else {
        m_ast = new astTU;
        m_scopes.push_back(scope());
        for (;;) {
            m_lexer.read(m_token, true);
            if (isType(kType_eof))
                break;

            std::vector<topLevel> items = parseTopLevel();

            if (isType(kType_semicolon)) {
                for (size_t i = 0; i < items.size(); i++) {
                    topLevel &parse = items[i];
                    astGlobalVariable *global = GC_NEW(astVariable) astGlobalVariable();
                    global->storage = parse.storage;
                    global->auxiliary = parse.auxiliary;
                    global->memory = parse.memory;
                    global->precision = parse.precision;
                    global->interpolation = parse.interpolation;
                    global->type = parse.type;
                    global->name = parse.name;
                    global->isArray = parse.isArray;
                    global->arraySizes = parse.arraySizes;
                    m_ast->globals.push_back(global);
                    m_scopes.back().push_back(global);
                }
            } else if (isOperator(kOperator_paranthesis_begin)) {
                m_ast->functions.push_back(parseFunction(items.front()));
            } else if (isType(kType_whitespace)) {
                continue; // whitespace tokens will be used later for the preprocessor
            } else {
                fatal("syntax error (top level)");
            }
        }
        return m_ast;
    }
    return 0;
}

void parser::parseStorage(topLevel &current) {
    // const, in, out, attribute, uniform, varying, buffer, shared
    if (isKeyword(kKeyword_const)) {
        current.storage = kConst;
        next(); // skip 'const'
    } else if (isKeyword(kKeyword_in)) {
        current.storage = kIn;
        next(); // skip 'in'
    } else if (isKeyword(kKeyword_out)) {
        current.storage = kOut;
        next(); // skip 'out'
    } else if (isKeyword(kKeyword_attribute)) {
        current.storage = kAttribute;
        next(); // skip 'attribute'
    } else if (isKeyword(kKeyword_uniform)) {
        current.storage = kUniform;
        next(); // skip 'uniform'
    } else if (isKeyword(kKeyword_varying)) {
        current.storage = kVarying;
        next(); // skip 'varying'
    } else if (isKeyword(kKeyword_buffer)) {
        current.storage = kBuffer;
        next(); // skip 'buffer'
    } else if (isKeyword(kKeyword_shared)) {
        current.storage = kShared;
        next(); // skip 'shared'
    }
}

void parser::parseAuxiliary(topLevel &current) {
    // centroid, sample, patch
    if (isKeyword(kKeyword_centroid)) {
        current.auxiliary = kCentroid;
        next(); // skip 'centroid'
    } else if (isKeyword(kKeyword_sample)) {
        current.auxiliary = kSample;
        next(); // skip 'sample'
    } else if (isKeyword(kKeyword_patch)) {
        current.auxiliary = kPatch;
        next(); // skip 'patch'
    }
}

void parser::parseInterpolation(topLevel &current) {
    // smooth, flat, noperspective
    if (isKeyword(kKeyword_smooth)) {
        current.interpolation = kSmooth;
        next(); // skip 'smooth'
    } else if (isKeyword(kKeyword_flat)) {
        current.interpolation = kFlat;
        next(); // skip 'flat'
    } else if (isKeyword(kKeyword_noperspective)) {
        current.interpolation = kNoPerspective;
        next(); // skip 'noperspective'
    }
}

void parser::parsePrecision(topLevel &current) {
    // highp, mediump, lowp
    if (isKeyword(kKeyword_highp)) {
        current.precision = kHighp;
        next(); // skip 'highp'
    } else if (isKeyword(kKeyword_mediump)) {
        current.precision = kMediump;
        next(); // skip 'mediump'
    } else if (isKeyword(kKeyword_lowp)) {
        current.precision = kLowp;
        next(); // skip 'lowp'
    }
}

void parser::parseInvariant(topLevel &current) {
    // invariant
    if (isKeyword(kKeyword_invariant)) {
        // TODO:
        next(); // skip 'invariant'
    }
}

void parser::parsePrecise(topLevel &current) {
    // precise
    if (isKeyword(kKeyword_precise)) {
        // TODO:
        next(); // skip 'precise'
    }
}

void parser::parseMemory(topLevel &current) {
    // coherent, volatile, restrict, readonly, writeonly
    if (isKeyword(kKeyword_coherent)) {
        current.memory |= kCoherent;
        next(); // skip 'coherent'
    } else if (isKeyword(kKeyword_volatile)) {
        current.memory |= kVolatile;
        next(); // skip 'volatile'
    } else if (isKeyword(kKeyword_restrict)) {
        current.memory |= kRestrict;
        next(); // skip 'restrict'
    } else if (isKeyword(kKeyword_readonly)) {
        current.memory |= kReadOnly;
        next(); // skip 'readonly'
    } else if (isKeyword(kKeyword_writeonly)) {
        current.memory |= kWriteOnly;
        next(); // skip 'writeonly;
    }
}

topLevel parser::parseTopLevelItem(topLevel *continuation) {
    std::vector<topLevel> items;
    while (!isBuiltin() && !isType(kType_identifier)) {
        topLevel next;
        if (continuation)
            next = *continuation;
        parseStorage(next);
        parseAuxiliary(next);
        parseInterpolation(next);
        parsePrecision(next);
        parseInvariant(next);
        parsePrecise(next);
        parseMemory(next);
        items.push_back(next);
    }

    topLevel level;
    if (continuation)
        level = *continuation;
    for (size_t i = 0; i < items.size(); i++) {
        topLevel &next = items[i];
        if (next.storage != -1 && level.storage != -1)
            fatal("multiple storage qualifiers in declaration");
        if (next.auxiliary != -1 && level.auxiliary != -1)
            fatal("multiple auxiliary storage qualifiers in declaration");
        if (next.interpolation != -1 && level.interpolation != -1)
            fatal("multiple interpolation qualifiers in declaration");
        if (next.precision != -1 && level.precision != -1)
            fatal("multiple precision qualifiers in declaration");
        level.storage = next.storage;
        level.auxiliary = next.auxiliary;
        level.interpolation = next.interpolation;
        level.precision = next.precision;
        level.memory |= next.memory;
    }

    if (!continuation) {
        if (isType(kType_identifier)) {
            level.type = findType(m_token.m_identifier);
            next(); // skip identifier
        } else {
            level.type = parseBuiltin();
            next(); // skip typename
        }

        if (level.type) {
            // Could be an array
            while (isOperator(kOperator_bracket_begin)) {
                level.isArray = true;
                level.arraySizes.insert(level.arraySizes.begin(), parseArraySize());
                next(); // skip ']'
            }
        }
    }

    if (!level.type)
        fatal("expected typename");

    if (isType(kType_identifier)) {
        level.name = m_token.m_identifier;
        next(); // skip identifier
    }

    while (isOperator(kOperator_bracket_begin)) {
        level.isArray = true;
        level.arraySizes.push_back(parseArraySize());
        next(); // skip ']'
    }

    return level;
}

std::vector<topLevel> parser::parseTopLevel() {
    std::vector<topLevel> items;
    items.push_back(parseTopLevelItem());
    while (isOperator(kOperator_comma)) {
        next(); // skip ','
        items.push_back(parseTopLevelItem(&items.front()));
    }
    return items;
}

void parser::parseLayout(std::vector<astLayoutQualifier*> &layoutQualifiers) {
    fatal("not implemented: layout qualifier parsing");
}

astStruct *parser::parseStruct() {
    fatal("not implemented: structure parsing");
    return 0;
}

astExpression *parser::parseBinary(int lhsPrecedence, astExpression *lhs, endCondition end) {
    // Precedence climbing
    while (!isEndCondition(end)) {
        int binaryPrecedence = m_token.precedence();
        if (binaryPrecedence < lhsPrecedence)
            break;

        astBinaryExpression *expression = createExpression();
        next();
        astExpression *rhs = parseUnary(end);
        next();

        int rhsPrecedence = m_token.precedence();
        // climb
        if (binaryPrecedence < rhsPrecedence)
            rhs = parseBinary(binaryPrecedence + 1, rhs, end);

        expression->operand1 = lhs;
        expression->operand2 = rhs;
        lhs = expression;
    }
    return lhs;
}

astExpression *parser::parseUnaryPrefix(endCondition condition) {
    if (isOperator(kOperator_paranthesis_begin)) {
        next(); // skip '('
        return parseExpression(kEndConditionParanthesis);
    } else if (isOperator(kOperator_logical_not)) {
        next(); // skip '!'
        return GC_NEW(astExpression) astUnaryLogicalNotExpression(parseUnary(condition));
    } else if (isOperator(kOperator_bit_not)) {
        next(); // skip '~'
        return GC_NEW(astExpression) astUnaryBitNotExpression(parseUnary(condition));
    } else if (isOperator(kOperator_plus)) {
        next(); // skip '+'
        return GC_NEW(astExpression) astUnaryPlusExpression(parseUnary(condition));
    } else if (isOperator(kOperator_minus)) {
        next(); // skip '-'
        return GC_NEW(astExpression) astUnaryMinusExpression(parseUnary(condition));
    } else if (isOperator(kOperator_increment)) {
        next(); // skip '++'
        return GC_NEW(astExpression) astPrefixIncrementExpression(parseUnary(condition));
    } else if (isOperator(kOperator_decrement)) {
        next(); // skip '--'
        return GC_NEW(astExpression) astPrefixDecrementExpression(parseUnary(condition));
    } else if (isBuiltin()) {
        return parseConstructorCall();
    } else if (isType(kType_identifier)) {
        token peek = m_lexer.peek();
        if (IS_OPERATOR(peek, kOperator_paranthesis_begin)) {
            astType *type = findType(m_token.m_identifier);
            if (type)
                return parseConstructorCall();
            else
                return parseFunctionCall();
        } else {
            astVariable *find = findVariable(m_token.m_identifier);
            if (find)
                return GC_NEW(astExpression) astVariableIdentifier(find);
            fatal("`%s' was not declared in this scope", m_token.m_identifier.c_str());
        }
    } else if (isKeyword(kKeyword_true))
        return GC_NEW(astExpression) astBoolConstant(true);
    else if (isKeyword(kKeyword_false))
        return GC_NEW(astExpression) astBoolConstant(false);
    else if (isType(kType_constant_int))
        return GC_NEW(astExpression) astIntConstant(m_token.asInt);
    else if (isType(kType_constant_uint))
        return GC_NEW(astExpression) astUIntConstant(m_token.asUnsigned);
    else if (isType(kType_constant_float))
        return GC_NEW(astExpression) astFloatConstant(m_token.asFloat);
    else if (isType(kType_constant_double))
        return GC_NEW(astExpression) astDoubleConstant(m_token.asDouble);
    else if (condition == kEndConditionBracket)
        return 0;
    fatal("syntax error");
    return 0;
}

astExpression *parser::parseUnary(endCondition end) {
    astExpression *operand = parseUnaryPrefix(end);
    for (;;) {
        token peek = m_lexer.peek();
        if (IS_OPERATOR(peek, kOperator_dot)) {
            next(); // skip last
            next(); // skip '.'
            if (!isType(kType_identifier))
                fatal("expected field identifier or swizzle after `.'");
            astFieldOrSwizzle *expression = GC_NEW(astExpression) astFieldOrSwizzle();
            expression->operand = operand;
            expression->name = peek.m_identifier;
            operand = expression;
        } else if (IS_OPERATOR(peek, kOperator_increment)) {
            next(); // skip last
            operand = GC_NEW(astExpression) astPostIncrementExpression(operand);
        } else if (IS_OPERATOR(peek, kOperator_decrement)) {
            next(); // skip last
            operand = GC_NEW(astExpression) astPostDecrementExpression(operand);
        } else if (IS_OPERATOR(peek, kOperator_bracket_begin)) {
            next(); // skip last
            next(); // skip '['
            astArraySubscript *expression = GC_NEW(astExpression) astArraySubscript();
            expression->operand = operand;
            expression->index = parseExpression(kEndConditionBracket);
            operand = expression;
        } else {
            break;
        }
    }
    return operand;
}

astExpression *parser::parseExpression(endCondition condition) {
    astExpression *lhs = parseUnary(condition);
    next(); // skip last
    return parseBinary(0, lhs, condition);
}

astExpressionStatement *parser::parseExpressionStatement(endCondition condition) {
    return GC_NEW(astStatement) astExpressionStatement(parseExpression(condition));
}

astConstantExpression *parser::parseArraySize() {
    next(); // skip '['
    return parseExpression(kEndConditionBracket);
}

astCompoundStatement *parser::parseCompoundStatement() {
    astCompoundStatement *statement = GC_NEW(astStatement) astCompoundStatement();
    next(); // skip '{'
    while (!isType(kType_scope_end)) {
        statement->statements.push_back(parseStatement());
        next(); // skip ';'
    }
    return statement;
}

astIfStatement *parser::parseIfStatement() {
    astIfStatement *statement = GC_NEW(astStatement) astIfStatement();
    next(); // skip 'if'
    if (!isOperator(kOperator_paranthesis_begin))
        fatal("expected `(' after `if' in if statement");
    next(); // skip '('
    statement->condition = parseExpression(kEndConditionParanthesis);
    next(); // skip ')'
    statement->thenStatement = parseStatement();
    token peek = m_lexer.peek();
    if (IS_KEYWORD(peek, kKeyword_else)) {
        next(); // skip ';' or '}'
        next(); // skip 'else'
        statement->elseStatement = parseStatement();
    }
    return statement;
}

astSwitchStatement *parser::parseSwitchStatement() {
    astSwitchStatement *statement = GC_NEW(astStatement) astSwitchStatement();
    next(); // skip 'switch'
    if (!isOperator(kOperator_paranthesis_begin))
        fatal("expected `(' after `switch' in switch statement");
    next(); // skip '('
    statement->expression = parseExpression(kEndConditionParanthesis);
    next(); // skip next
    if (!isType(kType_scope_begin))
        fatal("expected `{' after `)' in switch statement");
    next(); // skip '{'
    while (!isType(kType_scope_end)) {
        statement->statements.push_back(parseStatement());
        next();
    }
    // TODO: verify scope of where enum's are found
    return statement;
}

astCaseLabelStatement *parser::parseCaseLabelStatement() {
    astCaseLabelStatement *statement = GC_NEW(astStatement) astCaseLabelStatement();
    if (isKeyword(kKeyword_default)) {
        statement->isDefault = true;
        next(); // skip 'default'
        if (!isOperator(kOperator_colon))
            fatal("expected `:' after `default' in case label");
    } else {
        next(); // skip 'case'
        statement->condition = parseExpression(kEndConditionColon);
    }
    return statement;
}

astForStatement *parser::parseForStatement() {
    astForStatement *statement = GC_NEW(astStatement) astForStatement();
    next(); // skip 'for'
    if (!isOperator(kOperator_paranthesis_begin))
        fatal("expected `(' after `for' in for statement");
    next(); // skip '('
    if (!isType(kType_semicolon))
        statement->init = parseDeclarationOrExpressionStatement(kEndConditionSemicolon);
    next(); // skip ';'
    if (!isType(kType_semicolon))
        statement->condition = parseExpression(kEndConditionSemicolon);
    next(); // skip ';'
    if (!isOperator(kOperator_paranthesis_end))
        statement->loop = parseExpression(kEndConditionParanthesis);
    next(); // skip ')'
    statement->body = parseStatement();
    return statement;
}

astContinueStatement *parser::parseContinueStatement() {
    astContinueStatement *statement = GC_NEW(astStatement) astContinueStatement();
    next(); // skip 'continue'
    return statement;
}

astBreakStatement *parser::parseBreakStatement() {
    astBreakStatement *statement = GC_NEW(astStatement) astBreakStatement();
    next(); // skip 'break'
    return statement;
}

astDiscardStatement *parser::parseDiscardStatement() {
    astDiscardStatement *statement = GC_NEW(astStatement) astDiscardStatement();
    next(); // skip 'discard'
    return statement;
}

astReturnStatement *parser::parseReturnStatement() {
    astReturnStatement *statement = GC_NEW(astStatement) astReturnStatement();
    next(); // skip 'return'
    if (!isType(kType_semicolon))
        statement->expression = parseExpression(kEndConditionSemicolon);
    return statement;
}

astDoStatement *parser::parseDoStatement() {
    astDoStatement *statement = GC_NEW(astStatement) astDoStatement();
    next(); // skip 'do'
    statement->body = parseStatement();
    next();
    if (!isKeyword(kKeyword_while))
        fatal("expected `while' after `do' in do-while loop");
    next(); // skip 'while'
    if (!isOperator(kOperator_paranthesis_begin))
        fatal("expected `(' after `while' in do-while loop");
    next(); // skip '('
    statement->condition = parseExpression(kEndConditionParanthesis);
    next();
    return statement;
}

astWhileStatement *parser::parseWhileStatement() {
    astWhileStatement *statement = GC_NEW(astStatement) astWhileStatement();
    next(); // skip 'while'
    if (!isOperator(kOperator_paranthesis_begin))
        fatal("expected `(' after `while' in while-loop");
    next(); // skip '('
    statement->condition = parseDeclarationOrExpressionStatement(kEndConditionParanthesis);
    next();
    statement->body = parseStatement();
    return statement;
}

astDeclarationStatement *parser::parseDeclarationStatement(endCondition condition) {
    m_lexer.backup();

    bool isConst = false;
    if (isKeyword(kKeyword_const)) {
        isConst = true;
        next(); // skip 'const'
    }

    astType *type = 0;
    if (isBuiltin())
        type = parseBuiltin();
    else if (isType(kType_identifier))
        type = findType(m_token.m_identifier);

    if (!type) {
        m_lexer.restore();
        return 0;
    }

    next();

    astDeclarationStatement *statement = GC_NEW(astStatement) astDeclarationStatement();
    for (;;) {
        size_t paranthesisCount = 0;
        while (isOperator(kOperator_paranthesis_begin)) {
            paranthesisCount++;
            next(); // skip ','
        }
        if (!isType(kType_identifier)) {
            m_lexer.restore();
            return 0;
        }

        std::string name = m_token.m_identifier;
        next(); // skip identifier

        for (size_t i = 0; i < paranthesisCount; i++) {
            if (!isOperator(kOperator_paranthesis_end)) {
                m_lexer.restore();
                return 0;
            }
            next();
        }

        if (statement->variables.empty() && !isOperator(kOperator_assign)
            && !isOperator(kOperator_comma) && !isEndCondition(condition))
        {
            m_lexer.restore();
            return 0;
        }

        astExpression *initialValue = 0;
        if (isOperator(kOperator_assign)) {
            next(); // skip '='
            initialValue = parseExpression(kEndConditionComma | condition);
        }

        astFunctionVariable *variable = GC_NEW(astVariable) astFunctionVariable();
        variable->isConst = isConst;
        variable->type = type;
        variable->name = name;
        variable->initialValue = initialValue;
        statement->variables.push_back(variable);
        m_scopes.back().push_back(variable);

        if (isEndCondition(condition))
            break;
        else if (isOperator(kOperator_comma))
            next(); // skip ','
        else if (isOperator(kOperator_bracket_begin)) {
            while (isOperator(kOperator_bracket_begin)) {
                variable->isArray = true;
                variable->arraySizes.push_back(parseArraySize());
                next(); // skip ']'
            }
        } else {
            fatal("syntax error (declaration)");
        }
    }

    return statement;
}

astSimpleStatement *parser::parseDeclarationOrExpressionStatement(endCondition condition) {
    astSimpleStatement *declaration = parseDeclarationStatement(condition);
    if (declaration)
        return declaration;
    else
        return parseExpressionStatement(condition);
}

astStatement *parser::parseStatement() {
    if (isType(kType_scope_begin))
        return parseCompoundStatement();
    else if (isKeyword(kKeyword_if))
        return parseIfStatement();
    else if (isKeyword(kKeyword_switch))
        return parseSwitchStatement();
    else if (isKeyword(kKeyword_case) || isKeyword(kKeyword_default))
        return parseCaseLabelStatement();
    else if (isKeyword(kKeyword_for))
        return parseForStatement();
    else if (isKeyword(kKeyword_do))
        return parseDoStatement();
    else if (isKeyword(kKeyword_while))
        return parseWhileStatement();
    else if (isKeyword(kKeyword_continue))
        return parseContinueStatement();
    else if (isKeyword(kKeyword_break))
        return parseBreakStatement();
    else if (isKeyword(kKeyword_discard))
        return parseDiscardStatement();
    else if (isKeyword(kKeyword_return))
        return parseReturnStatement();
    else if (isType(kType_semicolon))
        return GC_NEW(astStatement) astEmptyStatement();
    else
        return parseDeclarationOrExpressionStatement(kEndConditionSemicolon);
}

astFunction *parser::parseFunction(const topLevel &parse) {
    astFunction *function = GC_NEW(astFunction) astFunction();
    function->returnType = parse.type;
    function->name = parse.name;

    next(); // skip '('
    while (!isOperator(kOperator_paranthesis_end)) {
        astFunctionParameter *parameter = GC_NEW(astVariable) astFunctionParameter();
        while (!isOperator(kOperator_comma) && !isOperator(kOperator_paranthesis_end)) {
            if (isKeyword(kKeyword_in))
                parameter->storage = kIn;
            else if (isKeyword(kKeyword_out))
                parameter->storage = kOut;
            else if (isKeyword(kKeyword_inout))
                parameter->storage = kInOut;
            else if (isKeyword(kKeyword_highp))
                parameter->precision = kHighp;
            else if (isKeyword(kKeyword_mediump))
                parameter->precision = kMediump;
            else if (isKeyword(kKeyword_lowp))
                parameter->precision = kLowp;
            else if (isKeyword(kKeyword_coherent))
                parameter->memory = kCoherent;
            else if (isKeyword(kKeyword_volatile))
                parameter->memory = kVolatile;
            else if (isKeyword(kKeyword_restrict))
                parameter->memory = kRestrict;
            else if (isKeyword(kKeyword_readonly))
                parameter->memory = kReadOnly;
            else if (isKeyword(kKeyword_writeonly))
                parameter->memory = kWriteOnly;
            else if (isType(kType_identifier)) {
                // TODO: user defined types
                parameter->name = m_token.m_identifier;
            } else if (isOperator(kOperator_bracket_begin)) {
                while (isOperator(kOperator_bracket_begin)) {
                    parameter->isArray = true;
                    parameter->arraySizes.push_back(parseArraySize());
                    //next(); // ']'
                }
            } else {
                parameter->type = parseBuiltin();
            }
            next();
        }

        if (!parameter->type)
            fatal("expected type");
        function->parameters.push_back(parameter);
        if (isOperator(kOperator_comma))
            next(); // skip ','
    }
    next(); // skip ')'

    if (isType(kType_scope_begin)) {
        function->isPrototype = false;
        next(); // skip '{'

        m_scopes.push_back(scope());
        for (size_t i = 0; i < function->parameters.size(); i++)
            m_scopes.back().push_back(function->parameters[i]);

        while (!isType(kType_scope_end)) {
            function->statements.push_back(parseStatement());
            next(); // skip ';'
        }

        m_scopes.pop_back();
    } else if (isType(kType_semicolon)) {
        function->isPrototype = true;
    } else {
        fatal("expected `{' or `;'");
    }
    return function;
}

// TODO: cleanup
#undef TYPENAME
#define TYPENAME(X) case kKeyword_##X:
astBuiltin *parser::parseBuiltin() {
    if (!isType(kType_keyword))
        fatal("expected keyword");

    switch (m_token.m_keyword) {
        #include "lexemes.h"
            for (size_t i = 0; i < m_builtins.size(); i++)
                if (m_builtins[i]->type == m_token.m_keyword)
                    return m_builtins[i];
            m_builtins.push_back(GC_NEW(astType) astBuiltin(m_token.m_keyword));
            return m_builtins.back();
            break;
        default:
            break;
    }
    fatal("internal compiler error: attempted to parse as builtin type");
    return 0;
}
#undef TYPENAME

astConstructorCall *parser::parseConstructorCall() {
    astConstructorCall *expression = GC_NEW(astExpression) astConstructorCall();
    expression->type = parseBuiltin();
    next();
    if (!isOperator(kOperator_paranthesis_begin))
        fatal("expected `(' in constructor call");
    next(); // skip '('
    while (!isOperator(kOperator_paranthesis_end)) {
        astExpression *parameter = parseExpression(kEndConditionComma | kEndConditionParanthesis);
        expression->parameters.push_back(parameter);
        if (isOperator(kOperator_comma))
            next(); // skip ','
    }
    return expression;
}

astFunctionCall *parser::parseFunctionCall() {
    astFunctionCall *expression = GC_NEW(astExpression) astFunctionCall();
    expression->name = m_token.m_identifier;
    next(); // skip identifier
    if (!isOperator(kOperator_paranthesis_begin))
        fatal("expected `(' in function call");
    next(); // skip '('
    while (!isOperator(kOperator_paranthesis_end)) {
        astExpression *parameter = parseExpression(kEndConditionComma | kEndConditionParanthesis);
        expression->parameters.push_back(parameter);
        if (isOperator(kOperator_comma))
            next(); // skip ','
    }
    return expression;
}

void parser::next() {
    m_lexer.read(m_token, true);
    if (isType(kType_eof))
        fatal("premature end of file");
}

astBinaryExpression *parser::createExpression() {
    if (!isType(kType_operator))
        fatal("internal compiler error: attempted to create binary expression in wrong context");

    switch (m_token.m_operator) {
        case kOperator_multiply:
        case kOperator_divide:
        case kOperator_modulus:
        case kOperator_plus:
        case kOperator_minus:
        case kOperator_shift_left:
        case kOperator_shift_right:
        case kOperator_less:
        case kOperator_greater:
        case kOperator_less_equal:
        case kOperator_greater_equal:
        case kOperator_equal:
        case kOperator_not_equal:
        case kOperator_bit_and:
        case kOperator_bit_xor:
        case kOperator_logical_and:
        case kOperator_logical_xor:
        case kOperator_logical_or:
            return GC_NEW(astExpression) astOperationExpression(m_token.m_operator);
        case kOperator_assign:
        case kOperator_add_assign:
        case kOperator_sub_assign:
        case kOperator_multiply_assign:
        case kOperator_divide_assign:
        case kOperator_modulus_assign:
        case kOperator_shift_left_assign:
        case kOperator_shift_right_assign:
        case kOperator_bit_and_assign:
        case kOperator_bit_xor_assign:
        case kOperator_bit_or_assign:
            return GC_NEW(astExpression) astAssignmentExpression(m_token.m_operator);
        case kOperator_comma:
            return GC_NEW(astExpression) astSequenceExpression();
        default:
            return 0;
    }
}

astType *parser::findType(const std::string &identifier) {
    //fatal("not implemented yet: custom types");
    return 0;
}

astVariable *parser::findVariable(const std::string &identifier) {
    for (size_t scopeIndex = m_scopes.size(); scopeIndex > 0; scopeIndex--) {
        scope &s = m_scopes[scopeIndex - 1];
        for (size_t variableIndex = 0; variableIndex < s.size(); variableIndex++)
            if (s[variableIndex]->name == identifier)
                return s[variableIndex];
    }
    return 0;
}

const char *parser::error() const {
    return m_error.c_str();
}

}
