#pragma once

#define MAX_CHAR 0x400

/* THE SCANNER */
typedef enum { EndT, StarT, ColonT, EqualT, BarT, IdenT, DotT } Lexical;

extern int LINE;
extern const char* LastW;
extern const char* ChArr;
extern char* ChP;

int OPEN(const char* path);

int CLOSE(void);

int GET(void);

void UNGET(int Ch);

Lexical LEX(int *errors);
