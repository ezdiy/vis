#ifndef TEXT_REGEX_H
#define TEXT_REGEX_H

/* make the REG_* constants available */
#if CONFIG_TRE
#include <tre/tre.h>
#else
#include <regex.h>
#endif
#include "text.h"
#include <limits.h>

struct bmh_t {
	char *pat;
	int patlen;
	int delta1[UCHAR_MAX + 1];
	int delta2;
};

#define MAX_REGEX_SUB 10

typedef struct Regex Regex;
typedef struct bmh_t bmh_t;
typedef Filerange RegexMatch;

Regex *text_regex_new(void);
int text_regex_compile(Regex*, const char *pattern, int cflags, bool slashmotion, bool fixedstring);
size_t text_regex_nsub(Regex*);
void text_regex_free(Regex*);
bool text_regex_is_slashmotion(Regex*);
int text_regex_match(Regex*, const char *data, int eflags);
int text_search_range_forward(Text*, size_t pos, size_t len, Regex *r, size_t nmatch, RegexMatch pmatch[], int eflags);
int text_search_range_backward(Text*, size_t pos, size_t len, Regex *r, size_t nmatch, RegexMatch pmatch[], int eflags);

#endif
