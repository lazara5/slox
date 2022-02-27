#ifndef SLOX_SCANNER_H
#define SLOX_SCANNER_H

#include <stdbool.h>

typedef enum {
	// Single-character tokens.
	TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN,
	TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,
	TOKEN_LEFT_BRACKET, TOKEN_RIGHT_BRACKET,
	TOKEN_COLON, TOKEN_COMMA, TOKEN_DOT, TOKEN_MINUS, TOKEN_PERCENT,
	TOKEN_PLUS, TOKEN_SEMICOLON, TOKEN_SLASH, TOKEN_STAR,
	// One or two character tokens.
	TOKEN_BANG, TOKEN_BANG_EQUAL,
	TOKEN_EQUAL, TOKEN_EQUAL_EQUAL,
	TOKEN_GREATER, TOKEN_GREATER_EQUAL,
	TOKEN_LESS, TOKEN_LESS_EQUAL,
	// Literals.
	TOKEN_IDENTIFIER, TOKEN_STRING, TOKEN_NUMBER,
	// Keywords.
	TOKEN_AND, TOKEN_BREAK, TOKEN_CATCH, TOKEN_CONTINUE,
	TOKEN_CLASS, TOKEN_ELSE, TOKEN_FALSE, TOKEN_FOR, TOKEN_FOREACH,
	TOKEN_FUNCTION, TOKEN_IF, TOKEN_NIL, TOKEN_OR, TOKEN_PRINT,
	TOKEN_RETURN, TOKEN_SUPER, TOKEN_THIS, TOKEN_THROW, TOKEN_TRUE,
	TOKEN_TRY, TOKEN_VAR, TOKEN_WHILE,

	TOKEN_ERROR, TOKEN_EOF
} TokenType;

typedef struct {
	TokenType type;
	const char *start;
	int length;
	int line;
} Token;

typedef struct {
	const char *start;
	char *current;
	int line;
} Scanner;

typedef struct VMCtx VMCtx;

void initScanner(VMCtx *vmCtx, char *source);
Token scanToken();

bool isAtEnd(Scanner *scanner);

#endif // SLOX_SCANNER_H
