/*  Copyright 2011, 2012 Jens Kehne
 *  Copyright 2012 Jan Stoess, Karlsruhe Institute of Technology
 *
 *  LibRIPC is free software: you can redistribute it and/or modify it under
 *  the terms of the GNU Lesser General Public License as published by the
 *  Free Software Foundation, either version 2.1 of the License, or (at your
 *  option) any later version.
 *
 *  LibRIPC is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 *  License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with libRIPC.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "common.h"
#include "memory.h"
#include "ripc.h"
#include <common.h>
#include <errno.h>
#include <memory.h>
#include <pthread.h>
#include <resolver.h>
#include <resources.h>
#include <ripc.h>
#include <string.h>
#include <string.h>
#define __USE_GNU
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

mem_buf_t ripc_alloc_recv_buf(size_t size) {
	if (!size)
		return invalid_mem_buf;

	if (! context.na.pd)
		init();
	assert(context.na.pd);

        mem_buf_t ret = free_buf_list_get(size);
	if (ret.size != -1) {
		DEBUG("Got hit in free list: Buffer at %p, size %zu", ret.na->addr, ret.na->length);
                used_buf_list_add(ret);
		memset(ret.na->addr, 0, ret.na->length);
                return ret;
	}

	//none found in cache, so create a new one
	DEBUG("No hit in free list, allocating new mr");
	//mmap correctly aligns and zeroes the buffer.
	void *buf = mmap(
                0,
                size,
                PROT_READ | PROT_WRITE,
                MAP_SHARED | MAP_ANONYMOUS,
                -1,
                0);

	if (buf == (void *) -1) {
		ERROR("buffer allocation failed: %s", strerror(errno));
		return invalid_mem_buf;
	}

	assert(buf);
	ret.na = ibv_reg_mr(
                context.na.pd,
                buf,
                size,
                IBV_ACCESS_LOCAL_WRITE |
                IBV_ACCESS_REMOTE_READ |
                IBV_ACCESS_REMOTE_WRITE);
	DEBUG("mr buffer address is %p, size %zu", ret.na->addr, ret.na->length);
        ret.addr = (uint64_t) ret.na->addr;
        ret.size = ret.na->length;
        used_buf_list_add(ret);
        memset(ret.na->addr, 0, ret.na->length);
        return ret;
}

mem_buf_t ripc_resize_recv_buf (mem_buf_t buf, size_t size) {
	ibv_dereg_mr(buf.na);
	void *ptr = mremap((void *)buf.addr, buf.size, size, MREMAP_MAYMOVE);
	if (ptr <= 0) {
		ERROR("mremap failed! oldaddr: %lu, newsize: %lu", buf.addr, size);
		munmap((void *)buf.addr, buf.size);
		return invalid_mem_buf;
	}
	mem_buf_t ret;
	ret.na = ibv_reg_mr(
                context.na.pd,
                ptr,
                size,
                IBV_ACCESS_LOCAL_WRITE |
                IBV_ACCESS_REMOTE_READ |
                IBV_ACCESS_REMOTE_WRITE);
    ret.addr = (uint64_t) ret.na->addr;
    ret.size = ret.na->length;
    used_buf_list_add(ret);
    return ret;
}

uint8_t ripc_buf_register(void *buf, size_t size) {
	DEBUG("Attempting buffer registration at %p, size %u", buf, size);

	if ( ! context.na.pd)
		init();

	mem_buf_t mem_buf =  used_buf_list_get(buf);
	if (mem_buf.size != -1) { // && ((uint64_t)buf + size > (uint64_t)mem_buf.addr + mem_buf.size)) {
		DEBUG("Buffer already registered at %p (requested: %p), size %u (requested: %u)", mem_buf.na->addr, buf, mem_buf.na->length, size);
		used_buf_list_add(mem_buf);
		return 0; //already registered
	}
	//if (mem_buf.size == -1 ) {
		mem_buf.na = ibv_reg_mr(
                        context.na.pd,
                        buf,
                        size,
                        IBV_ACCESS_LOCAL_WRITE |
                        IBV_ACCESS_REMOTE_READ |
                        IBV_ACCESS_REMOTE_WRITE);
                if (mem_buf.na) {
                        mem_buf.addr = (uint64_t) mem_buf.na->addr;
                        mem_buf.size = mem_buf.na->length;
                        used_buf_list_add(mem_buf);
                        DEBUG("Buffer registered successfully at %p, size %u", mem_buf.na->addr, mem_buf.na->length);
                        return 0; //registration successful
                }

    //    }
	return 1; //registration unsuccessful
}

uint8_t ripc_buf_unregister(void *buf) {
	DEBUG("Attempting buffer deregistration at address %p", buf);

	mem_buf_t mem_buf = used_buf_list_get(buf);
	if (mem_buf.size == -1) {
		DEBUG("Buffer %p does not exist", buf);
		return 0;
	}
	int ret = ibv_dereg_mr(mem_buf.na);
	if(ret) {
		ERROR("Failed to deregister buffer at %p: %s", buf, strerror(ret));
		return 0;
	}
	return 1;
}

void post_new_recv_buf(struct ibv_qp *qp) {
	struct ibv_sge *list;
	struct ibv_recv_wr *wr, *bad_wr;
	uint32_t i;

        mem_buf_t mem_buf = ripc_alloc_recv_buf(RECV_BUF_SIZE);

	list = malloc(sizeof(struct ibv_sge));
	list->addr = mem_buf.addr;
	list->length = mem_buf.size;
	list->lkey = mem_buf.na->lkey;

	wr = malloc(sizeof(struct ibv_recv_wr));
	wr->wr_id = (uint64_t)wr;
	wr->sg_list = list;
	wr->num_sge = 1;
	wr->next = NULL;

	if (ibv_post_recv(qp, wr, &bad_wr)) {
		ERROR("Failed to post receive item to QP %u!", qp->qp_num);
	} else {
		DEBUG("Posted receive buffer at address %lx to QP %u",
                      mem_buf.addr, qp->qp_num);
	}
}

void dump_mem_buf(mem_buf_t *buf) {
	ERROR("addr: %#lx", buf->addr);
	ERROR("size: %zd", buf->size);
	ERROR("rcv_addr: %p", buf->rcv_addr);
	ERROR("rcv_size: %zd", buf->rcv_size);
	ERROR("netarch info at %p", buf->na);
}
