#include <stdio.h>
#include <string.h>

#include "slox/common.h"
#include "slox/scanner.h"

typedef struct {
	const char *start;
	char *current;
	int line;
} Scanner;

static Scanner scanner;

void initScanner(char *source) {
	scanner.start = source;
	scanner.current = source;
	scanner.line = 1;
}

static bool isAlpha(char c) {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool isDigit(char c) {
	return c >= '0' && c <= '9';
}

static bool isAtEnd() {
	return *scanner.current == '\0';
}

static char peek() {
	return *scanner.current;
}

static char peekNext() {
	if (isAtEnd())
		return '\0';
	return scanner.current[1];
}

static char advance() {
	scanner.current++;
	return scanner.current[-1];
}

static char *getPos() {
	return scanner.current;
}

static bool match(char expected) {
	if (isAtEnd())
		return false;
	if (*scanner.current != expected)
		return false;
	scanner.current++;
	return true;
}

static Token makeToken(TokenType type) {
	Token token;
	token.type = type;
	token.start = scanner.start;
	token.length = (int)(scanner.current - scanner.start);
	token.line = scanner.line;
	return token;
}

static Token makeTrimmedToken(TokenType type, int len) {
	Token token;
	token.type = type;
	token.start = scanner.start;
	token.length = len;
	token.line = scanner.line;
	return token;
}

// TODO: combine
static Token errorToken(const char *message) {
	Token token;
	token.type = TOKEN_ERROR;
	token.start = message;
	token.length = (int)strlen(message);
	token.line = scanner.line;
	return token;
}

typedef enum {
	WSS_SCAN,
	WSS_STAR,
	WSS_DONE
} WSSState;

static bool skipWhitespace() {
	for (;;) {
		char c = peek();
		switch (c) {
			case ' ':
			case '\r':
			case '\t':
				advance();
				break;
			case '\n':
				scanner.line++;
				advance();
				break;
			case '/':
				if (peekNext() == '/') {
					// A comment goes until the end of the line.
					while (peek() != '\n' && !isAtEnd())
						advance();
				} else if (peekNext() == '*') {
					// multi-line comment
					advance();
					WSSState state = WSS_SCAN;
					while (!isAtEnd()) {
						char ch = advance();
						switch (state) {
							case WSS_SCAN:
								switch (ch) {
									case '*':
										state = WSS_STAR;
										break;
									case '\n':
										scanner.line++;
										break;
								}
								break;
							case WSS_STAR:
								switch (ch) {
									case '/':
										state = WSS_DONE;
										break;
									case '\n':
										scanner.line++;
										// FALLTHROUGH
									default:
										state = WSS_SCAN;
										break;
								}
								break;
							case WSS_DONE:
								// Unreachable
								break;
						}
						if (state == WSS_DONE)
							break;
					}
					if (state != WSS_DONE)
						return false;
				} else {
					return true;
				}
				break;
			default:
				return true;
		}
	}
}

static TokenType checkKeyword(int start, int length, const char *rest, TokenType type) {
	if (scanner.current - scanner.start == start + length &&
			memcmp(scanner.start + start, rest, (size_t)length) == 0) {
		return type;
	}

	return TOKEN_IDENTIFIER;
}

static TokenType identifierType() {
	switch (scanner.start[0]) {
		case 'a':
			return checkKeyword(1, 2, "nd", TOKEN_AND);
		case 'b':
			return checkKeyword(1, 4, "reak", TOKEN_BREAK);
		case 'c':
			if (scanner.current - scanner.start > 1) {
				switch (scanner.start[1]) {
					case 'a':
						return checkKeyword(2, 3, "tch", TOKEN_CATCH);
					case 'l':
						return checkKeyword(2, 3, "ass", TOKEN_CLASS);
					case 'o':
						return checkKeyword(2, 6, "ntinue", TOKEN_CONTINUE);
				}
			}
			break;
		case 'e':
			return checkKeyword(1, 3, "lse", TOKEN_ELSE);
		case 'f':
			if (scanner.current - scanner.start > 1) {
				switch (scanner.start[1]) {
					case 'a':
						return checkKeyword(2, 3, "lse", TOKEN_FALSE);
					case 'o':
						return checkKeyword(2, 1, "r", TOKEN_FOR);
					case 'u':
						return checkKeyword(2, 1, "n", TOKEN_FUN);
				}
			}
			break;
		case 'i':
			return checkKeyword(1, 1, "f", TOKEN_IF);
		case 'n':
			return checkKeyword(1, 2, "il", TOKEN_NIL);
		case 'o':
			return checkKeyword(1, 1, "r", TOKEN_OR);
		case 'p':
			return checkKeyword(1, 4, "rint", TOKEN_PRINT);
		case 'r':
			return checkKeyword(1, 5, "eturn", TOKEN_RETURN);
		case 's':
			return checkKeyword(1, 4, "uper", TOKEN_SUPER);
		case 't':
			if (scanner.current - scanner.start > 1) {
				switch (scanner.start[1]) {
					case 'h':
						if (scanner.current - scanner.start > 1) {
							switch (scanner.start[2]) {
								case 'i':
									return checkKeyword(3, 1, "s", TOKEN_THIS);
								case 'r':
									return checkKeyword(3, 2, "ow", TOKEN_THROW);
							}
						}

					case 'r':
						if (scanner.current - scanner.start > 1) {
							switch (scanner.start[2]) {
								case 'u':
									return checkKeyword(3, 1, "e", TOKEN_TRUE);
								case 'y':
									return checkKeyword(3, 0, "", TOKEN_TRY);
							}
						}

				}
			}
			break;
		case 'v':
			return checkKeyword(1, 2, "ar", TOKEN_VAR);
		case 'w':
			return checkKeyword(1, 4, "hile", TOKEN_WHILE);
	}

	return TOKEN_IDENTIFIER;
}

static Token identifier() {
	while (isAlpha(peek()) || isDigit(peek()))
		advance();
	return makeToken(identifierType());
}

static Token number() {
	while (isDigit(peek()))
		advance();

	// Look for a fractional part.
	if (peek() == '.' && isDigit(peekNext())) {
		// Consume the ".".
		advance();

		while (isDigit(peek()))
			advance();
	}

	return makeToken(TOKEN_NUMBER);
}

static Token string(char delimiter) {
	typedef enum {
		SCAN, ESCAPE
	} SSMODE;

	bool complete = false;
	char *start = getPos();
	char *output = start;
	SSMODE mode = SCAN;
	do {
		char ch = advance();
		switch (mode) {
			case SCAN: {
				if (ch == delimiter) {
					*(output++) = ch;
					complete = true;
				} else if (ch == '\\')
					mode = ESCAPE;
				else if (ch == '\0')
					return errorToken("Unterminated string");
				else
					*(output++) = ch;
				break;
			}
			case ESCAPE: {
				switch (ch) {
					case '\'':
					case '\\':
					case '"':
						*(output++) = ch;
						mode = SCAN;
						break;
					case 'n':
						*(output++) = '\n';
						mode = SCAN;
						break;
					case 'r':
						*(output++) = '\r';
						mode = SCAN;
						break;
					case 't':
						*(output++) = '\t';
						mode = SCAN;
						break;
					case '\0':
						return errorToken("Unterminated string");
					default:
						return errorToken("Invalid escape sequence");
				}

				break;
			}
		}
	} while (!complete);
	int len = output - start;
	return makeTrimmedToken(TOKEN_STRING, len + 1);
}

Token scanToken() {
	if (!skipWhitespace())
		return errorToken("Unterminated comment");

	scanner.start = scanner.current;

	if (isAtEnd())
		return makeToken(TOKEN_EOF);

	char c = advance();
	if (isAlpha(c))
		return identifier();
	if (isDigit(c))
		return number();

	switch (c) {
		case '(':
			return makeToken(TOKEN_LEFT_PAREN);
		case ')':
			return makeToken(TOKEN_RIGHT_PAREN);
		case '{':
			return makeToken(TOKEN_LEFT_BRACE);
		case '}':
			return makeToken(TOKEN_RIGHT_BRACE);
		case '[':
			return makeToken(TOKEN_LEFT_BRACKET);
		case ']':
			return makeToken(TOKEN_RIGHT_BRACKET);
		case ':':
			return makeToken(TOKEN_COLON);
		case ';':
			return makeToken(TOKEN_SEMICOLON);
		case ',':
			return makeToken(TOKEN_COMMA);
		case '.':
			return makeToken(TOKEN_DOT);
		case '-':
			return makeToken(TOKEN_MINUS);
		case '+':
			return makeToken(TOKEN_PLUS);
		case '/':
			return makeToken(TOKEN_SLASH);
		case '%':
			return makeToken(TOKEN_PERCENT);
		case '*':
			return makeToken(TOKEN_STAR);
		case '!':
			return makeToken(match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
		case '=':
			return makeToken(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
		case '<':
			return makeToken(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
		case '>':
			return makeToken(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
		case '"': return string('"');
		case '\'': return string('\'');
	}

	return errorToken("Unexpected character.");
}
