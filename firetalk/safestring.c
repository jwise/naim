/*
safestring.c - FireTalk replacement string functions
Copyright (C) 2000 Ian Gulliver

This program is free software; you can redistribute it and/or modify
it under the terms of version 2 of the GNU General Public License as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/
#include "safestring.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>

void *safe_malloc(const size_t size) {
	char *output;
	output = malloc(size);
	if (output == NULL) {
		perror("malloc");
		abort();
	}
	return output;
}

void *safe_realloc(void *old, const size_t new) {
	void *output;
	output = realloc(old,new);
	if (output == NULL) {
		perror("realloc");
		abort();
	}
	return output;
}

char *safe_strdup(const char * const input) {
	char *output;
	size_t s;
	if (input == NULL)
		return NULL;
	s = strlen(input) + 1;
	output = safe_malloc(s);
	safe_strncpy(output,input,s);
	return(output);
}

void safe_strncpy(char * const to, const char * const from, const size_t size) {
	strncpy(to, from, size-1);
	to[size - 1]= '\0';
}

void safe_strncat(char * const to, const char * const from, const size_t size) {
	size_t	l = strlen(to);

	safe_strncpy(&to[l], from, size - l);
}

void safe_snprintf(char *out, const size_t size, char * const format, ...) {
	va_list ap;
	char numbuf[10]; /* stores shorts for printing */
	size_t f,o = 0,fl,tl,ml;
	char *tempchr;
	int b = 0;

	fl = strlen(format);
	ml = size - 1;

	va_start(ap,format);
	for (f = 0; f < fl && o < ml && b == 0; f++) {
		if (format[f] == '%') {
			switch(format[++f]) {
				case 's':
					tempchr = va_arg(ap,char *);
					tl = strlen(tempchr);
					if (tl + o >= ml)
						b = 1;
					else {
						memcpy(&out[o],tempchr,tl);
						out += tl;
					}
					break;
				case 'd':
					sprintf(numbuf,"%d",va_arg(ap,int));
					tl = strlen(numbuf);
					if (tl + o >= ml)
						b = 1;
					else {
						memcpy(&out[o],numbuf,tl);
						out += tl;
					}
					break;
				case '%':
					out[o++] = '%';
					break;
			}
		} else
			out[o++] = format[f];
	}
	va_end(ap);
	out[o] = '\0';
	return;
}
