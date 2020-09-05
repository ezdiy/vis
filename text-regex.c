#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "text-regex.h"

struct Regex {
	regex_t regex;
	Text *text;
	Iterator it;
	size_t end;
	bmh_t bmh;
	bool slashmotion;
	bool fixedstring;
};

Regex *text_regex_new(void) {
	Regex *r = calloc(1, sizeof(Regex));
	if (!r)
		return NULL;
	regcomp(&r->regex, "\0\0", 0); /* this should not match anything */
	return r;
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
	int r = regcomp(&regex->regex, string, cflags);
	if (r)
		regcomp(&regex->regex, "\0\0", 0);
	regex->slashmotion = slashmotion;
	regex->fixedstring = fixedstring;
	bmh_init(string, strlen(string), &regex->bmh);
	return r;
}

size_t text_regex_nsub(Regex *r) {
	if (!r)
		return 0;
	return r->regex.re_nsub;
}

void text_regex_free(Regex *r) {
	if (!r)
		return;
	regfree(&r->regex);
	free(r->bmh.pat);
	free(r);
}

bool text_regex_is_slashmotion(Regex *r) {
	return r->slashmotion;
}

int text_regex_match(Regex *r, const char *data, int eflags) {
	return regexec(&r->regex, data, 0, NULL, eflags);
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
	if (r->fixedstring && r->slashmotion) {
		r->text = txt;
		r->it = text_iterator_get(txt, pos);
		r->end = pos+len;
		ret = bmh_memmem(r);
		if (!ret) {
			pmatch[0].start = r->it.pos;
			pmatch[0].end = pmatch[0].start + r->bmh.patlen;
		}
	} else {
		char *buf = text_bytes_alloc0(txt, pos, len);
		if (!buf)
			return REG_NOMATCH;
		char *cur = buf, *end = buf + len;
		regmatch_t match[MAX_REGEX_SUB];
		for (size_t junk = len; len > 0; len -= junk, pos += junk) {
			ret = regexec(&r->regex, cur, nmatch, match, eflags);
			if (!ret) {
				for (size_t i = 0; i < nmatch; i++) {
					pmatch[i].start = match[i].rm_so == -1 ? EPOS : pos + match[i].rm_so;
					pmatch[i].end = match[i].rm_eo == -1 ? EPOS : pos + match[i].rm_eo;
				}
				break;
			}
			char *next = memchr(cur, 0, len);
			if (!next)
				break;
			while (!*next && next != end)
				next++;
			junk = next - cur;
			cur = next;
		}
		free(buf);
	}
	return ret;
}

int text_search_range_backward(Text *txt, size_t pos, size_t len, Regex *r, size_t nmatch, RegexMatch pmatch[], int eflags) {
	char *buf = text_bytes_alloc0(txt, pos, len);
	if (!buf)
		return REG_NOMATCH;
	char *cur = buf, *end = buf + len;
	int ret = REG_NOMATCH;
	regmatch_t match[MAX_REGEX_SUB];
	for (size_t junk = len; len > 0; len -= junk, pos += junk) {
		char *next;
		if (!regexec(&r->regex, cur, nmatch, match, eflags)) {
			ret = 0;
			for (size_t i = 0; i < nmatch; i++) {
				pmatch[i].start = match[i].rm_so == -1 ? EPOS : pos + match[i].rm_so;
				pmatch[i].end = match[i].rm_eo == -1 ? EPOS : pos + match[i].rm_eo;
			}

			if (match[0].rm_so == 0 && match[0].rm_eo == 0) {
				/* empty match at the beginning of cur, advance to next line */
				next = strchr(cur, '\n');
				if (!next)
					break;
				next++;
			} else {
				next = cur + match[0].rm_eo;
			}
		} else {
			next = memchr(cur, 0, len);
			if (!next)
				break;
			while (!*next && next != end)
				next++;
		}
		junk = next - cur;
		cur = next;
		if (cur[-1] == '\n')
			eflags &= ~REG_NOTBOL;
		else
			eflags |= REG_NOTBOL;
	}
	free(buf);
	return ret;
}
