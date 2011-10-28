/*  _ __   __ _ ___ __  __
** | '_ \ / _` |_ _|  \/  | naim
** | | | | (_| || || |\/| | Copyright 1998-2003 Daniel Reed <n@ml.org>
** |_| |_|\__,_|___|_|  |_| ncurses-based chat client
*/
#ifndef modutil_h
#define modutil_h	1

#include <naim/naim.h>
#include <fcntl.h>
#include <time.h>
#include <ltdl.h>
#include <string.h>

typedef int (*mod_hook_t)(void *userdata, ...);
typedef struct {
	int	count;
	struct {
		int	weight;
		unsigned long passes, hits;
		mod_hook_t func;
		char	*name;
		void	*userdata, *mod;
	} *hooks;
} chain_t;

static inline chain_t *_hook_findchain(const char *name) {
	extern lt_dlhandle dl_self;
	chain_t	*hook;
	char	buf[100];

	snprintf(buf, sizeof(buf), "chain_%s", name);

	hook = lt_dlsym(dl_self, buf);
	return(hook);
}

#define HOOK_JUMP	1
#define HOOK_STOP	0
#define HOOK_CONTINUE	(-1)

#define HOOK_T_CONN	"c"
#define HOOK_T_CONNc	'c'
#define HOOK_T_FDSET	"f"
#define HOOK_T_FDSETc	'f'
#define HOOK_T_TIME	"t"
#define HOOK_T_TIMEc	't'
#define HOOK_T_HANDLE	"v"
#define HOOK_T_HANDLEc	'v'
#define HOOK_T_STRING	"s"
#define HOOK_T_STRINGc	's'
#define HOOK_T_LSTRING	"l"
#define HOOK_T_LSTRINGc	'l'
#define HOOK_T_UINT32	"u"
#define HOOK_T_UINT32c	'u'
#define HOOK_T_FLOAT	"d"
#define HOOK_T_FLOATc	'd'
#define HOOK_T_WRSTRING	"S"
#define HOOK_T_WRSTRINGc 'S'
#define HOOK_T_WRLSTRING "L"
#define HOOK_T_WRLSTRINGc 'L'
#define HOOK_T_WRUINT32	"U"
#define HOOK_T_WRUINT32c 'U'
#define HOOK_T_WRFLOAT	"D"
#define HOOK_T_WRFLOATc 'D'

#define HOOK_DECLARE(x)	chain_t chain_ ## x = { 0 }
#define HOOK_EXT_L(x)	extern chain_t chain_ ## x
#define HOOK_CALL(x, ...)					\
	if ((chain_ ## x).count > 0) do { 			\
		int	i;					\
		for (i = 0; (i < chain_ ## x.count)		\
			&& (chain_ ## x.hooks[i].passes++ || 1)	\
			&& ((chain_ ## x.hooks[i].func(chain_ ## x.hooks[i].userdata, ##__VA_ARGS__) == HOOK_CONTINUE) \
			 || (chain_ ## x.hooks[i].hits++ && 0)); i++); \
	} while (0)
#define HOOK_ADD(x, m, f, w, u)					\
	do { 							\
		HOOK_EXT_L(x);					\
		int	i;					\
								\
		HOOK_DEL(x, m, f, u);				\
		for (i = 0; (i < chain_ ## x.count)		\
			&& (chain_ ## x.hooks[i].weight <= w); i++); \
		HOOK_INS(chain_ ## x, m, f, w, u, i);		\
	} while (0)
#define HOOK_ADD2(name, m, f, w, u)				\
	do { 							\
		chain_t *chain_ptr = _hook_findchain(name);	\
		int	i;					\
								\
		HOOK_DEL2(name, m, f, u);			\
		for (i = 0; (i < chain_ptr->count)		\
			&& (chain_ptr->hooks[i].weight <= w); i++); \
		HOOK_INS((*chain_ptr), m, f, w, u, i);	\
	} while (0)
#define HOOK_INS(chain, m, f, w, u, pos)			\
	do {							\
		if (pos > chain.count)				\
			pos = chain.count;			\
		chain.hooks = realloc(chain.hooks,		\
			(chain.count+1)*sizeof(*(chain.hooks))); \
		memmove(chain.hooks+pos+1, chain.hooks+pos,	\
			(chain.count-pos)*sizeof(*(chain.hooks))); \
		chain.hooks[pos].weight = w;			\
		chain.hooks[pos].passes = 0;			\
		chain.hooks[pos].hits = 0;			\
		chain.hooks[pos].func = (mod_hook_t)f;		\
		chain.hooks[pos].name = strdup(#f);		\
		chain.hooks[pos].userdata = u;			\
		chain.hooks[pos].mod = m;			\
		chain.count++;					\
	} while (0)
#define HOOK_DEL(x, m, f, u)					\
	do { 							\
		HOOK_EXT_L(x);					\
								\
		HOOK_DODEL(chain_ ## x, m, f, u);		\
	} while (0)
#define HOOK_DEL2(name, m, f, u)				\
	do {							\
		chain_t *chain_ptr = _hook_findchain(name);	\
								\
		HOOK_DODEL((*chain_ptr), m, f, u);		\
	} while (0)
#define HOOK_DODEL(chain, m, f, u)				\
	do { 							\
		int	i;					\
								\
		for (i = 0; (i < chain.count)			\
			&& ((chain.hooks[i].mod != m)		\
			 || (chain.hooks[i].func != (mod_hook_t)f) \
			 || (chain.hooks[i].userdata != u)); i++); \
		if (i < chain.count) {				\
			free(chain.hooks[i].name);		\
			memmove(chain.hooks+i, chain.hooks+i+1,	\
				(chain.count-i-1)*sizeof(*(chain.hooks))); \
			chain.hooks = realloc(chain.hooks, \
				(chain.count-1)*sizeof(*(chain.hooks))); \
			chain.count--;				\
		}						\
	} while (0)

#define MOD_REMAINLOADED	1
#define MOD_FINISHED		0

#define MODULE_LICENSE(x)	const char module_license[] = #x
#define MODULE_AUTHOR(x)	const char module_author[] = #x
#define MODULE_CATEGORY(x)	const char module_category[] = #x
#define MODULE_DESCRIPTION(x)	const char module_description[] = #x
#define MODULE_VERSION(x)	const double module_version = x

#endif /* naim_h */
