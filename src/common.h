#ifndef COMMON_H_
#define COMMON_H_

#include "config.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <infiniband/verbs.h>

#define ERROR(...) fprintf(stderr, "%s() (%s, line %u): ", __PRETTY_FUNCTION__, __FILE__, __LINE__); fprintf(stderr, __VA_ARGS__); fprintf(stderr,"\n")
#define panic(...) fprintf(stderr, "%s() (%s, line %u): FATAL: ", __PRETTY_FUNCTION__, __FILE__, __LINE__); fprintf(stderr, __VA_ARGS__); fprintf(stderr,"\n"); exit(EXIT_FAILURE)

#ifdef HAVE_DEBUG
#define DEBUG(...) ERROR(__VA_ARGS__)
#else
#define DEBUG(...)
#endif

#ifdef HAVE_STDBOOL_H
# include <stdbool.h>
#else
# ifndef HAVE__BOOL
#  ifdef __cplusplus
typedef bool _Bool;
#  else
#   define _Bool signed char
#  endif
# endif
# define bool _Bool
# define false 0
# define true 1
# define __bool_true_false_are_defined 1
#endif

struct service_id {
	uint16_t number;
	struct ibv_cq *cq;
	struct ibv_qp *qp;
	struct ibv_comp_channel *cchannel;
};

struct library_context {
	struct ibv_context *device_context;
	struct service_id *services[UINT16_MAX];
	struct ibv_pd *pd;
};

enum msg_type {
	RIPC_MSG_SEND,
	RIPC_MSG_INTERRUPT,
	RIPC_MSG_RESOLVE_REQ,
	RIPC_MST_RESOLVE_REPLY
};

struct msg_header {
	enum msg_type type;
	uint16_t from;
	uint16_t to;
	uint16_t short_words;
	uint16_t long_words;
	uint16_t return_bufs;
};

struct short_header {
	uint32_t offset;
	uint32_t size;
};

extern struct library_context context;

#endif /* COMMON_H_ */
