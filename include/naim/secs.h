/*  ___  ___  ___ ___
** / __|/ _ \/ __/ __| secs
** \__ \  __/ (__\__ \ Copyright 1999 Daniel Reed <n@ml.org>
** |___/\___|\___|___/ Simple Embedded Client Scripting
*/
#ifndef secs_h
#define secs_h  1

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SECS_ERROR	(-1)
#define SECS_CONTINUE	0
#define SECS_HANDLED	1

typedef struct secs_var_s {
	char	*name,
		*val;
	size_t	length;
	struct secs_var_s	*next;
	int	(*set)(struct secs_var_s *, const char *);
} secs_var_t;

typedef struct secs_block_s {
	char	*name,
		*script;
	secs_var_t	*variables;
	struct secs_block_s	*parent;
} secs_block_t;

/* From `atomizer.c': */
char * firstatom (char *string , char *bounds );
char * firstwhite (char *string );
char * atom (char *string );
/* From `block.c': */
void secs_block_init (void);
secs_block_t * secs_block_create (secs_block_t *parent , char *name );
int secs_block_var_add (secs_block_t *block , secs_var_t *var );

#if 0
secs_block_t * secs_block_find (secs_block_t *first , char *name );

#endif
int secs_block_free (secs_block_t *block );
/* From `liaison.c': */
secs_block_t * secs_block_getroot (void);
int secs_init (void);
void secs_handle (char *line );
int secs_makevar (const char *name , const char *value , char type );
int secs_setvar (const char *name , const char *val );
char * secs_getvar (const char *name );
int secs_getvar_int (const char *name );
char * secs_listvars (int i , size_t *length, void **_var );
/* From `mem.c': */
void secs_mem_init (void);
void * secs_mem_alloc (size_t size );
void * secs_mem_realloc (void *ptr , size_t size );
void secs_mem_free (void *ptr );
/* From `script.c': */
void secs_script_init (void);
char * secs_script_expand (secs_block_t *block , const char *instr );
int secs_script_parse (const char *line );
/* From `vars.c': */
void secs_var_init (void);
int secs_var_set_str (secs_var_t *var , const char *val );
int secs_var_set_int (secs_var_t *var , const char *value );
int secs_var_set_switch (secs_var_t *var , const char *val );
int secs_var_set (secs_var_t *var , const char *val );
secs_var_t * secs_var_find (secs_var_t *first , const char *name );
secs_var_t * secs_var_find_n (secs_var_t *first , const char *name );
secs_var_t * secs_var_create (const char *name , int ( *varset ) ( secs_var_t * , const char * ) );
int secs_var_free (secs_var_t *var );

#endif
