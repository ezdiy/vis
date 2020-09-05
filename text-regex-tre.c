#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <errno.h>
#include <limits.h>

#include "text-regex.h"
#include "text-motions.h"

struct Regex {
	regex_t regex;
	tre_str_source str_source;
	Text *text;
	Iterator it;
	size_t end;
	bmh_t bmh;
	bool slashmotion;
	bool fixedstring;
};

size_t text_regex_nsub(Regex *r) {
       if (!r)
               return 0;
       return r->regex.re_nsub;
}

static int str_next_char(tre_char_t *c, unsigned int *pos_add, void *context) {
	Regex *r = context;
	Iterator *it = &r->it;
	if (TRE_WCHAR) {
		mbstate_t ps = { 0 };
		bool eof = false;
		size_t start = it->pos;
		for (;;) {
			if (it->pos >= r->end) {
				eof = true;
				break;
			}
			size_t rem = r->end - it->pos;
			size_t plen = it->end - it->text;
			size_t len = rem < plen ? rem : plen;
			size_t wclen = mbrtowc(c, it->text, len, &ps);
			if (wclen == (size_t)-1 && errno == EILSEQ) {
				ps = (mbstate_t){0};
				*c = L'\0';
				text_iterator_codepoint_next(it, NULL);
				break;
			} else if (wclen == (size_t)-2) {
				if (!text_iterator_next(it)) {
					eof = true;
					break;
				}
			} else if (wclen == 0) {
				text_iterator_byte_next(it, NULL);
				break;
			} else {
				if (wclen < plen) {
					it->text += wclen;
					it->pos += wclen;
				} else {
					text_iterator_next(it);
				}
				break;
			}
		}

		if (eof) {
			*c = L'\0';
			*pos_add = 1;
			return 1;
		} else {
			*pos_add = it->pos - start;
			return 0;
		}
	} else {
		*pos_add = 1;
		if (it->pos < r->end && text_iterator_byte_get(it, (char*)c)) {
			text_iterator_byte_next(it, NULL);
			return 0;
		} else {
			*c = '\0';
			return 1;
		}
	}
}

static void str_rewind(size_t pos, void *context) {
	Regex *r = context;
	r->it = text_iterator_get(r->text, pos);
}

static int str_compare(size_t pos1, size_t pos2, size_t len, void *context) {
	Regex *r = context;
	int ret = 1;
	void *buf1 = malloc(len), *buf2 = malloc(len);
	if (!buf1 || !buf2)
		goto err;
	text_bytes_get(r->text, pos1, len, buf1);
	text_bytes_get(r->text, pos2, len, buf2);
	ret = memcmp(buf1, buf2, len);
err:
	free(buf1);
	free(buf2);
	return ret;
}

Regex *text_regex_new(void) {
	Regex *r = calloc(1, sizeof(*r));
	if (!r)
		return NULL;
	r->str_source = (tre_str_source) {
		.get_next_char = str_next_char,
		.rewind = str_rewind,
		.compare = str_compare,
		.context = r,
	};
	return r;
}

void text_regex_free(Regex *r) {
	if (!r)
		return;
	tre_regfree(&r->regex);
	free(r->bmh.pat);
	free(r);
}

static void bmh_init(const char *pat, int patlen, bmh_t *bmh) {
	for (int i = 0; i <= UCHAR_MAX; ++i)
		bmh->delta1[i] = patlen;
	for (int i = 0; i < patlen; ++i)
		bmh->delta1[(uint8_t)pat[i]] = patlen - i - 1;
	char lastpatchar = pat[patlen - 1];
	bmh->delta2 = patlen;
	for (int i = 0; i < patlen - 1; ++i)
		if (pat[i] == lastpatchar)
			bmh->delta2 = patlen - i - 1;
	free(bmh->pat);
	bmh->pat = pat ? strdup(pat) : NULL;
	bmh->patlen = patlen;
}

int text_regex_compile(Regex *regex, const char *string, int cflags, bool slashmotion, bool fixedstring) {
	int r = tre_regcomp(&regex->regex, string, cflags);
	if (r)
		tre_regcomp(&regex->regex, "\0\0", 0);
	regex->slashmotion = slashmotion;
	regex->fixedstring = fixedstring;
	bmh_init(string, strlen(string), &regex->bmh);
	return r;
}

bool text_regex_is_slashmotion(Regex *r) {
	return r->slashmotion;
}

int text_regex_match(Regex *r, const char *data, int eflags) {
	return tre_regexec(&r->regex, data, 0, NULL, eflags);
}

static int bmh_memmem(Regex *r) {
	char *pat = r->bmh.pat;
	int patlen = r->bmh.patlen;
	if (patlen == 0)
		return REG_NOMATCH;
	int *delta1 = r->bmh.delta1;
	int delta2 = r->bmh.delta2;
	int j;
	char c;
	size_t shift;
	Iterator *it = &r->it;
	bool ret = text_iterator_bytes_skip(r->text, it, patlen - 1);
	for ( ;; ) {
		while (ret && it->pos < r->end && text_iterator_byte_get(it, &c) && (shift = delta1[(uint8_t)c]) > 0)
			ret = text_iterator_bytes_skip(r->text, it, shift);
		if (!ret || it->pos >= r->end)
			return REG_NOMATCH;
		j = patlen - 1;
		while (--j >= 0 && text_iterator_byte_prev(it, &c) && c == pat[j])
			;
		if (j < 0)
			return 0;
		ret = text_iterator_bytes_skip(r->text, it, delta2 + patlen - 1 - j);
	}
}

int text_search_range_forward(Text *txt, size_t pos, size_t len, Regex *r, size_t nmatch, RegexMatch pmatch[], int eflags) {
	int ret = REG_NOMATCH;
	r->text = txt;
	r->it = text_iterator_get(txt, pos);
	r->end = pos+len;

	if (r->fixedstring && r->slashmotion) {
		ret = bmh_memmem(r);
		if (!ret) {
			pmatch[0].start = r->it.pos;
			pmatch[0].end = pmatch[0].start + r->bmh.patlen;
		}
	} else {
		regmatch_t match[MAX_REGEX_SUB];
		ret = tre_reguexec(&r->regex, &r->str_source, nmatch, match, eflags);
		if (!ret) {
			for (size_t i = 0; i < nmatch; i++) {
				pmatch[i].start = match[i].rm_so == -1 ? EPOS : pos + match[i].rm_so;
				pmatch[i].end = match[i].rm_eo == -1 ? EPOS : pos + match[i].rm_eo;
			}
		}
	}
	return ret;
}

int text_search_range_backward(Text *txt, size_t pos, size_t len, Regex *r, size_t nmatch, RegexMatch pmatch[], int eflags) {
	int ret = REG_NOMATCH;
	size_t end = pos + len;

	while (pos < end && !text_search_range_forward(txt, pos, len, r, nmatch, pmatch, eflags)) {
		ret = 0;
		// FIXME: assumes nmatch >= 1
		size_t next = pmatch[0].end;
		if (next == pos) {
			next = text_line_next(txt, pos);
			if (next == pos)
				break;
		}
		pos = next;
		len = end - pos;

		char c;
		if (text_byte_get(txt, pos-1, &c) && c == '\n')
			eflags &= ~REG_NOTBOL;
		else
			eflags |= REG_NOTBOL;
	}

	return ret;
}
