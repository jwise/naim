/*  ___  ___  ___ ___
** / __|/ _ \/ __/ __| secs
** \__ \  __/ (__\__ \ Copyright 1999-2003 Daniel Reed <n@ml.org>
** |___/\___|\___|___/ Simple Embedded Client Scripting
*/
#include <naim/secs.h>

void	secs_block_init(void) {
}

secs_block_t	*secs_block_create(secs_block_t *parent, char *name) {
	secs_block_t	*block = NULL;
	size_t	slen = 0;

	assert(name != NULL);

	slen = strlen(name);
	block = secs_mem_alloc(sizeof(secs_block_t));
	block->name = secs_mem_alloc(slen+1);
	strncpy(block->name, name, slen+1);
	block->script = NULL;
	block->variables = NULL;
	block->parent = parent;
	return(block);
}

int	secs_block_var_add(secs_block_t *block, secs_var_t *var) {
	assert(block != NULL);
	assert(var != NULL);

	if (var->next != NULL) {
		secs_var_t	*varptr = var->next;

		while (varptr->next != NULL)
			varptr = varptr->next;
		varptr->next = block->variables;
	} else
		var->next = block->variables;
	block->variables = var;
	return(1);
}

#if 0
secs_block_t	*secs_block_find(secs_block_t *first, char *name) {
	register secs_block_t	*block = first;

	assert(name != NULL);
	if (block == NULL)
		return(NULL);
	do {
		if (strcasecmp(block->name, name) == 0)
			return(block);
	} while ((block = block->next) != NULL);
	return(NULL);
}
#endif

int	secs_block_free(secs_block_t *block) {
	if (block == NULL)
		return(0);
	if (block->name != NULL)
		secs_mem_free(block->name);
	if (block->script != NULL)
		secs_mem_free(block->script);
	while (block->variables != NULL) {
		secs_var_t	*var = block->variables;

		block->variables = var->next;
		secs_var_free(var);
	}
	return(1);
}
