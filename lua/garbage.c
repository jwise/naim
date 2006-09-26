/*  _ __   __ _ ___ __  __
** | '_ \ / _` |_ _|  \/  | naim
** | | | | | | || || |\/| | Copyright 1998-2006 Daniel Reed <n@ml.org>
** |_| |_|\__,_|___|_|  |_| ncurses-based chat client
*/

#include <stdlib.h>
#include "moon-int.h"

static void *_garbage_dump[100] = { 0 }, **_garbage_dump2 = NULL;
static int _garbage_count = 0, _garbage_count2 = 0;

void	nlua_clean_garbage(void) {
	int	i;

	if (_garbage_count > 0) {
		for (i = 0; i < _garbage_count; i++) {
			free(_garbage_dump[i]);
			_garbage_dump[i] = NULL;
		}
		_garbage_count = 0;
	}
	if (_garbage_count2 > 0) {
		for (i = 0; i < _garbage_count2; i++) {
			free(_garbage_dump2[i]);
			_garbage_dump2[i] = NULL;
		}
		free(_garbage_dump2);
		_garbage_dump2 = NULL;
		_garbage_count2 = 0;
	}
}

void	_garbage_add(void *ptr) {
	if (_garbage_count < sizeof(_garbage_dump)/sizeof(*_garbage_dump))
		_garbage_dump[_garbage_count++] = ptr;
	else {
		_garbage_dump2 = realloc(_garbage_dump2, (_garbage_count2+1)*sizeof(*_garbage_dump2));
		if (_garbage_dump2 == NULL)
			abort();
		_garbage_dump2[_garbage_count2++] = ptr;
	}
}
