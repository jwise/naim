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

void safe_strncpy(char *const to, const char *const from, const size_t size) {
	strncpy(to, from, size-1);
	to[size - 1]= '\0';
}

void safe_strncat(char *const to, const char *const from, const size_t size) {
	size_t	l = strlen(to);

	safe_strncpy(&to[l], from, size - l);
}
