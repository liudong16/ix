/*
 * Copyright 2013-16 Board of Trustees of Stanford University
 * Copyright 2013-16 Ecole Polytechnique Federale Lausanne (EPFL)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/*
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
 * All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 * 
 * Author: Adam Dunkels <adam@sics.se>
 *
 */
#ifndef __LWIP_TCP_IMPL_H__
#define __LWIP_TCP_IMPL_H__

#include "lwip/opt.h"

#if LWIP_TCP /* don't build if not configured for use in lwipopts.h */

#include "lwip/tcp.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/ip.h"
#include "lwip/icmp.h"
#include "lwip/err.h"
#include "lwip/ip6.h"
#include "lwip/ip6_addr.h"

#include <ix/hash.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Functions for interfacing with TCP: */

/* Lower layer interface to TCP: */
void             tcp_init    (struct eth_fg *);  /* Initialize this module. */


/* Only used by IP to pass a TCP segment to TCP: */
	void             tcp_input   (struct eth_fg *cur_fg, struct pbuf *p, ipX_addr_t *,ipX_addr_t *);
/* Used within the TCP code only: */
struct tcp_pcb * tcp_alloc   (struct eth_fg *,u8_t prio);
void             tcp_abandon (struct eth_fg *,struct tcp_pcb *pcb, int reset);
err_t            tcp_send_empty_ack(struct eth_fg *,struct tcp_pcb *pcb);
void             tcp_rexmit  (struct tcp_pcb *pcb);
void             tcp_rexmit_rto  (struct eth_fg *cur_fg,struct tcp_pcb *pcb);
void             tcp_rexmit_fast (struct tcp_pcb *pcb);
u32_t            tcp_update_rcv_ann_wnd(struct tcp_pcb *pcb);
err_t            tcp_process_refused_data(struct eth_fg *,struct tcp_pcb *pcb);

/**
 * This is the Nagle algorithm: try to combine user data to send as few TCP
 * segments as possible. Only send if
 * - no previously transmitted data on the connection remains unacknowledged or
 * - the TF_NODELAY flag is set (nagle algorithm turned off for this pcb) or
 * - the only unsent segment is at least pcb->mss bytes long (or there is more
 *   than one unsent segment - with lwIP, this can happen although unsent->len < mss)
 * - or if we are in fast-retransmit (TF_INFR)
 */
#define tcp_do_output_nagle(tpcb) ((((tpcb)->unacked == NULL) || \
                            ((tpcb)->flags & (TF_NODELAY | TF_INFR)) || \
                            (((tpcb)->unsent != NULL) && (((tpcb)->unsent->next != NULL) || \
                              ((tpcb)->unsent->len >= (tpcb)->mss))) || \
                            ((tcp_sndbuf(tpcb) == 0) || (tcp_sndqueuelen(tpcb) >= TCP_SND_QUEUELEN)) \
                            ) ? 1 : 0)
#define tcp_output_nagle(tpcb) (tcp_do_output_nagle(tpcb) ? tcp_output(cur_fg,tpcb) : ERR_OK)


#define TCP_SEQ_LT(a,b)     ((s32_t)((u32_t)(a) - (u32_t)(b)) < 0)
#define TCP_SEQ_LEQ(a,b)    ((s32_t)((u32_t)(a) - (u32_t)(b)) <= 0)
#define TCP_SEQ_GT(a,b)     ((s32_t)((u32_t)(a) - (u32_t)(b)) > 0)
#define TCP_SEQ_GEQ(a,b)    ((s32_t)((u32_t)(a) - (u32_t)(b)) >= 0)
/* is b<=a<=c? */
#if 0 /* see bug #10548 */
#define TCP_SEQ_BETWEEN(a,b,c) ((c)-(b) >= (a)-(b))
#endif
#define TCP_SEQ_BETWEEN(a,b,c) (TCP_SEQ_GEQ(a,b) && TCP_SEQ_LEQ(a,c))
#define TCP_FIN 0x01U
#define TCP_SYN 0x02U
#define TCP_RST 0x04U
#define TCP_PSH 0x08U
#define TCP_ACK 0x10U
#define TCP_URG 0x20U
#define TCP_ECE 0x40U
#define TCP_CWR 0x80U

#define TCP_FLAGS 0x3fU

/* Length of the TCP header, excluding options. */
#define TCP_HLEN 20

#ifndef TCP_TMR_INTERVAL
#define TCP_TMR_INTERVAL       250  /* The TCP timer interval in milliseconds. */
#endif /* TCP_TMR_INTERVAL */

#ifndef TCP_FAST_INTERVAL
#define TCP_FAST_INTERVAL      TCP_TMR_INTERVAL /* the fine grained timeout in milliseconds */
#endif /* TCP_FAST_INTERVAL */

#ifndef TCP_SLOW_INTERVAL
#define TCP_SLOW_INTERVAL      (2*TCP_TMR_INTERVAL)  /* the coarse grained timeout in milliseconds */
#endif /* TCP_SLOW_INTERVAL */

#define TCP_FIN_WAIT_TIMEOUT 20000 /* milliseconds */
#define TCP_SYN_RCVD_TIMEOUT 20000 /* milliseconds */

#define TCP_OOSEQ_TIMEOUT        6U /* x RTO */

#ifndef TCP_MSL
#define TCP_MSL 60000UL /* The maximum segment lifetime in milliseconds */
#endif

/* Keepalive values, compliant with RFC 1122. Don't change this unless you know what you're doing */
#ifndef  TCP_KEEPIDLE_DEFAULT
#define  TCP_KEEPIDLE_DEFAULT     7200000UL /* Default KEEPALIVE timer in milliseconds */
#endif

#ifndef  TCP_KEEPINTVL_DEFAULT
#define  TCP_KEEPINTVL_DEFAULT    75000UL   /* Default Time between KEEPALIVE probes in milliseconds */
#endif

#ifndef  TCP_KEEPCNT_DEFAULT
#define  TCP_KEEPCNT_DEFAULT      9U        /* Default Counter for KEEPALIVE probes */
#endif

#define  TCP_MAXIDLE              TCP_KEEPCNT_DEFAULT * TCP_KEEPINTVL_DEFAULT  /* Maximum KEEPALIVE probe time */

/* Fields are (of course) in network byte order.
 * Some fields are converted to host byte order in tcp_input().
 */
#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/bpstruct.h"
#endif
PACK_STRUCT_BEGIN
struct tcp_hdr {
  PACK_STRUCT_FIELD(u16_t src);
  PACK_STRUCT_FIELD(u16_t dest);
  PACK_STRUCT_FIELD(u32_t seqno);
  PACK_STRUCT_FIELD(u32_t ackno);
  PACK_STRUCT_FIELD(u16_t _hdrlen_rsvd_flags);
  PACK_STRUCT_FIELD(u16_t wnd);
  PACK_STRUCT_FIELD(u16_t chksum);
  PACK_STRUCT_FIELD(u16_t urgp);
} PACK_STRUCT_STRUCT;
PACK_STRUCT_END
#ifdef PACK_STRUCT_USE_INCLUDES
#  include "arch/epstruct.h"
#endif

#define TCPH_HDRLEN(phdr) (ntohs((phdr)->_hdrlen_rsvd_flags) >> 12)
#define TCPH_FLAGS(phdr)  (ntohs((phdr)->_hdrlen_rsvd_flags) & TCP_FLAGS)

#define TCPH_HDRLEN_SET(phdr, len) (phdr)->_hdrlen_rsvd_flags = htons(((len) << 12) | TCPH_FLAGS(phdr))
#define TCPH_FLAGS_SET(phdr, flags) (phdr)->_hdrlen_rsvd_flags = (((phdr)->_hdrlen_rsvd_flags & PP_HTONS((u16_t)(~(u16_t)(TCP_FLAGS)))) | htons(flags))
#define TCPH_HDRLEN_FLAGS_SET(phdr, len, flags) (phdr)->_hdrlen_rsvd_flags = htons(((len) << 12) | (flags))

#define TCPH_SET_FLAG(phdr, flags ) (phdr)->_hdrlen_rsvd_flags = ((phdr)->_hdrlen_rsvd_flags | htons(flags))
#define TCPH_UNSET_FLAG(phdr, flags) (phdr)->_hdrlen_rsvd_flags = htons(ntohs((phdr)->_hdrlen_rsvd_flags) | (TCPH_FLAGS(phdr) & ~(flags)) )

#define TCP_TCPLEN(seg) ((seg)->len + (((TCPH_FLAGS((seg)->tcphdr) & (TCP_FIN | TCP_SYN)) != 0) ? 1U : 0U))

/** Flags used on input processing, not on pcb->flags
*/
#define TF_RESET     (u8_t)0x08U   /* Connection was reset. */
#define TF_CLOSED    (u8_t)0x10U   /* Connection was sucessfully closed. */
#define TF_GOT_FIN   (u8_t)0x20U   /* Connection was closed by the remote end. */


#if LWIP_EVENT_API

/* IX WARNING: cur_fg implied in macro (in other places as well) */
#define TCP_EVENT_ACCEPT(pcb,err,ret)    ret = lwip_tcp_event(cur_fg,(pcb)->callback_arg, (pcb), \
                LWIP_EVENT_ACCEPT, NULL, 0, err)
#define TCP_EVENT_SENT(pcb,space,ret) ret = lwip_tcp_event(cur_fg,(pcb)->callback_arg, (pcb), \
                   LWIP_EVENT_SENT, NULL, space, ERR_OK)
#define TCP_EVENT_RECV(pcb,p,err,ret) ret = lwip_tcp_event(cur_fg,(pcb)->callback_arg, (pcb), \
                LWIP_EVENT_RECV, (p), 0, (err))
#define TCP_EVENT_CLOSED(pcb,ret) ret = lwip_tcp_event(cur_fg,(pcb)->callback_arg, (pcb), \
                LWIP_EVENT_RECV, NULL, 0, ERR_OK)
#define TCP_EVENT_CONNECTED(pcb,err,ret) ret = lwip_tcp_event(cur_fg,(pcb)->callback_arg, (pcb), \
                LWIP_EVENT_CONNECTED, NULL, 0, (err))
#define TCP_EVENT_POLL(pcb,ret)       ret = lwip_tcp_event(cur_fg,(pcb)->callback_arg, (pcb), \
                LWIP_EVENT_POLL, NULL, 0, ERR_OK)
#define TCP_EVENT_ERR(errf,arg,err)  lwip_tcp_event(cur_fg,(arg), NULL, \
                LWIP_EVENT_ERR, NULL, 0, (err))

#else /* LWIP_EVENT_API */

#define TCP_EVENT_ACCEPT(pcb,err,ret)                          \
  do {                                                         \
    if((pcb)->accept != NULL)                                  \
      (ret) = (pcb)->accept((pcb)->callback_arg,(pcb),(err));  \
    else (ret) = ERR_ARG;                                      \
  } while (0)

#define TCP_EVENT_SENT(pcb,space,ret)                          \
  do {                                                         \
    if((pcb)->sent != NULL)                                    \
      (ret) = (pcb)->sent((pcb)->callback_arg,(pcb),(space));  \
    else (ret) = ERR_OK;                                       \
  } while (0)

#define TCP_EVENT_RECV(pcb,p,err,ret)                          \
  do {                                                         \
    if((pcb)->recv != NULL) {                                  \
      (ret) = (pcb)->recv((pcb)->callback_arg,(pcb),(p),(err));\
    } else {                                                   \
      (ret) = tcp_recv_null(NULL, (pcb), (p), (err));          \
    }                                                          \
  } while (0)

#define TCP_EVENT_CLOSED(pcb,ret)                                \
  do {                                                           \
    if(((pcb)->recv != NULL)) {                                  \
      (ret) = (pcb)->recv((pcb)->callback_arg,(pcb),NULL,ERR_OK);\
    } else {                                                     \
      (ret) = ERR_OK;                                            \
    }                                                            \
  } while (0)

#define TCP_EVENT_CONNECTED(pcb,err,ret)                         \
  do {                                                           \
    if((pcb)->connected != NULL)                                 \
      (ret) = (pcb)->connected((pcb)->callback_arg,(pcb),(err)); \
    else (ret) = ERR_OK;                                         \
  } while (0)

#define TCP_EVENT_POLL(pcb,ret)                                \
  do {                                                         \
    if((pcb)->poll != NULL)                                    \
      (ret) = (pcb)->poll((pcb)->callback_arg,(pcb));          \
    else (ret) = ERR_OK;                                       \
  } while (0)

#define TCP_EVENT_ERR(errf,arg,err)                            \
  do {                                                         \
    if((errf) != NULL)                                         \
      (errf)((arg),(err));                                     \
  } while (0)

#endif /* LWIP_EVENT_API */

/** Enabled extra-check for TCP_OVERSIZE if LWIP_DEBUG is enabled */
#if TCP_OVERSIZE && defined(LWIP_DEBUG)
#define TCP_OVERSIZE_DBGCHECK 1
#else
#define TCP_OVERSIZE_DBGCHECK 0
#endif

/** Don't generate checksum on copy if CHECKSUM_GEN_TCP is disabled */
#define TCP_CHECKSUM_ON_COPY  (LWIP_CHECKSUM_ON_COPY && CHECKSUM_GEN_TCP)

/* This structure represents a TCP segment on the unsent, unacked and ooseq queues */
struct tcp_seg {
  struct tcp_seg *next;    /* used when putting segements on a queue */
  struct pbuf *p;          /* buffer containing data + TCP header */
  u16_t len;               /* the TCP length of this segment */
#if TCP_OVERSIZE_DBGCHECK
  u16_t oversize_left;     /* Extra bytes available at the end of the last
                              pbuf in unsent (used for asserting vs.
                              tcp_pcb.unsent_oversized only) */
#endif /* TCP_OVERSIZE_DBGCHECK */ 
#if TCP_CHECKSUM_ON_COPY
  u16_t chksum;
  u8_t  chksum_swapped;
#endif /* TCP_CHECKSUM_ON_COPY */
  u8_t  flags;
#define TF_SEG_OPTS_MSS         (u8_t)0x01U /* Include MSS option. */
#define TF_SEG_OPTS_TS          (u8_t)0x02U /* Include timestamp option. */
#define TF_SEG_DATA_CHECKSUMMED (u8_t)0x04U /* ALL data (not the header) is
                                               checksummed into 'chksum' */
#define TF_SEG_OPTS_WND_SCALE   (u8_t)0x08U /* Include WND SCALE option */
  struct tcp_hdr *tcphdr;  /* the TCP header */
};

#define LWIP_TCP_OPT_LEN_MSS  4
#if LWIP_TCP_TIMESTAMPS
#define LWIP_TCP_OPT_LEN_TS   12
#else
#define LWIP_TCP_OPT_LEN_TS   0
#endif
#if LWIP_WND_SCALE
#define LWIP_TCP_OPT_LEN_WS   4
#else
#define LWIP_TCP_OPT_LEN_WS   0
#endif

#define LWIP_TCP_OPT_LENGTH(flags) \
  (flags & TF_SEG_OPTS_MSS       ? LWIP_TCP_OPT_LEN_MSS : 0) + \
  (flags & TF_SEG_OPTS_TS        ? LWIP_TCP_OPT_LEN_TS  : 0) + \
  (flags & TF_SEG_OPTS_WND_SCALE ? LWIP_TCP_OPT_LEN_WS  : 0)

/** This returns a TCP header option for MSS in an u32_t */
#define TCP_BUILD_MSS_OPTION(mss) htonl(0x02040000 | ((mss) & 0xFFFF))

#if LWIP_WND_SCALE
#define TCPWNDSIZE_F  U32_F
#define TCPWND_MAX    0xFFFFFFFFU
#else /* LWIP_WND_SCALE */
#define TCPWNDSIZE_F  U16_F
#define TCPWND_MAX    0xFFFFU
#endif /* LWIP_WND_SCALE */

/* Global variables: */
DECLARE_PERCPU(struct tcp_pcb *, tcp_input_pcb);

/* The TCP PCB lists. */

/**
 * big changes from original LWIP:
 *
 * all active pcbs are accessed via a hash function.
 * the pcbs form a doubly-linked list (all belonging to the same hash value)
 * hash bucktets themselves are linked together (head = tcp_active_pcbs) via a singly-linked list of
 * entries with >0 pcbs. (updated lazily).
 */



union tcp_listen_pcbs_t { /* List of all TCP PCBs in LISTEN state. */
  struct tcp_pcb_listen *listen_pcbs; 
  struct tcp_pcb *pcbs;
};


#define TCP_ACTIVE_PCBS_HASH_SEED 0xa36bdcbe


struct tcp_global_percpu_lists {
	struct hlist_head listen_pcbs;    // tcp_pcb
	int nothing;
};

DECLARE_PERCPU(struct tcp_global_percpu_lists,tcp_cpu_lists);

static inline int tcp_to_idx(ipX_addr_t *local_ip, ipX_addr_t *remote_ip, uint16_t local_port, uint16_t remote_port)
{
  int idx = hash_crc32c_two(TCP_ACTIVE_PCBS_HASH_SEED, local_ip->addr, remote_ip->addr);
  idx = hash_crc32c_one(idx, (local_port << 16) | remote_port);
  idx &= TCP_ACTIVE_PCBS_MAX_BUCKETS - 1;
  return idx;
}


/* Axioms about the above lists:   
   1) Every TCP PCB that is not CLOSED is in one of the lists.
   2) A PCB is only in one of the lists.
   3) All PCBs in the tcp_listen_pcbs list is in LISTEN state.
   4) All PCBs in the tcp_tw_pcbs list is in TIME-WAIT state.
*/
/* Define two macros, TCP_REG and TCP_RMV that registers a TCP PCB
   with a PCB list or removes a PCB from a list, respectively. */
#ifndef TCP_DEBUG_PCB_LISTS
#define TCP_DEBUG_PCB_LISTS 0
#endif
#if TCP_DEBUG_PCB_LISTS
#error "not working in IX"
#define TCP_REG(pcbs, npcb) do {\
                            LWIP_DEBUGF(TCP_DEBUG, ("TCP_REG %p local port %d\n", (npcb), (npcb)->local_port)); \
                            for(tcp_tmp_pcb = *(pcbs); \
          tcp_tmp_pcb != NULL; \
        tcp_tmp_pcb = tcp_tmp_pcb->next) { \
                                LWIP_ASSERT("TCP_REG: already registered\n", tcp_tmp_pcb != (npcb)); \
                            } \
                            LWIP_ASSERT("TCP_REG: pcb->state != CLOSED", ((pcbs) == &tcp_bound_pcbs) || ((npcb)->state != CLOSED)); \
                            (npcb)->next = *(pcbs); \
                            LWIP_ASSERT("TCP_REG: npcb->next != npcb", (npcb)->next != (npcb)); \
                            *(pcbs) = (npcb); \
                            LWIP_ASSERT("TCP_RMV: tcp_pcbs sane", tcp_pcbs_sane()); \
              tcp_timer_needed(); \
                            } while(0)
#define TCP_RMV(pcbs, npcb) do { \
                            LWIP_ASSERT("TCP_RMV: pcbs != NULL", *(pcbs) != NULL); \
                            LWIP_DEBUGF(TCP_DEBUG, ("TCP_RMV: removing %p from %p\n", (npcb), *(pcbs))); \
                            if(*(pcbs) == (npcb)) { \
                               *(pcbs) = (*pcbs)->next; \
                            } else for(struct tcp_pcb *tcp_tmp_pcb = *(pcbs); tcp_tmp_pcb != NULL; tcp_tmp_pcb = tcp_tmp_pcb->next) { \
                               if(tcp_tmp_pcb->next == (npcb)) { \
                                  tcp_tmp_pcb->next = (npcb)->next; \
                                  break; \
                               } \
                            } \
                            (npcb)->next = NULL; \
                            LWIP_ASSERT("TCP_RMV: tcp_pcbs sane", tcp_pcbs_sane()); \
                            LWIP_DEBUGF(TCP_DEBUG, ("TCP_RMV: removed %p from %p\n", (npcb), *(pcbs))); \
                            } while(0)

#else /* LWIP_DEBUG */

void tcp_send_delayed_ack(struct timer *t);
void tcp_retransmit_handler(struct timer *t);
void tcp_persist_handler(struct timer *t);
	extern void tcp_unified_timer_handler(struct timer *t, struct eth_fg *cur_fg);

#define TCP_REG(pcbs, npcb,cur_fg)		   \
  do {                                             \
      assert((npcb)->link.prev == NULL);           \
	hlist_add_head(pcbs,&(npcb)->link);	     \
	/* (npcb)->perqueue = percpu_get(current_perqueue);*/		\
	timer_init_entry(&(npcb)->unified_timer, tcp_unified_timer_handler); \
    tcp_timer_needed(cur_fg);                            \
  } while (0)

/** 
 * __TCP_RMV -- complement to TCP_REG and TCP_REG_ACTIVE
 */

static inline void __TCP_RMV(struct eth_fg *cur_fg,struct tcp_pcb *pcb)
{
	struct tcp_hash_entry *he = &cur_fg->active_tbl[0];
	struct tcp_hash_entry *he2;
	/* if you are removing the only entry in the hash list, remove the bucket from the bucket list */
	if ((uintptr_t) pcb->link.prev >= (uintptr_t)he && 
	    (uintptr_t) pcb->link.prev < (uintptr_t)&he[TCP_ACTIVE_PCBS_MAX_BUCKETS] &&
	    pcb->link.next == NULL) {
		he2 = (struct tcp_hash_entry *) pcb->link.prev;
		int idx = he2 - he;
		assert (idx>=0 && idx<TCP_ACTIVE_PCBS_MAX_BUCKETS);
		assert ((uintptr_t)he + idx * sizeof(struct tcp_hash_entry) == (uintptr_t)he2);
		
		hlist_del(&he2->hash_link);
		he2->hash_link.prev = NULL;
	}
	hlist_del(&pcb->link);		
	pcb->link.prev = NULL;
//	pcb->perqueue = NULL;
	timer_del(&pcb->unified_timer);
}


#define TCP_RMV(pcbs, npcb)   __TCP_RMV(cur_fg,npcb)

#define TCP_HASH_RMV(pcbs,npcb)

#define TCP_HASH_RMV_NOT_USED(pcbs, npcb)                   \
  do {                                             \
    int idx = tcp_to_idx(&npcb->local_ip, &npcb->remote_ip, npcb->local_port, npcb->remote_port); \
    if((pcbs)[idx] == (npcb)) {                    \
      (pcbs)[idx] = (pcbs)[idx]->hash_bucket_next; \
    }                                              \
    else {                                         \
      struct tcp_pcb *tcp_tmp_pcb;                 \
      for(tcp_tmp_pcb = (pcbs)[idx];               \
          tcp_tmp_pcb != NULL;                     \
          tcp_tmp_pcb = tcp_tmp_pcb->hash_bucket_next) { \
        if(tcp_tmp_pcb->hash_bucket_next == (npcb)) { \
          tcp_tmp_pcb->hash_bucket_next = (npcb)->hash_bucket_next; \
          break;                                   \
        }                                          \
      }                                            \
    }                                              \
    (npcb)->hash_bucket_next = NULL;               \
  } while(0)

#endif /* LWIP_DEBUG */


static inline void tcp_timer_needed(struct eth_fg *cur_fg)
{
	/* timer is off but needed again? */

	if (!timer_pending(&cur_fg->tcpip_timer) && 
	    (!hlist_empty(&cur_fg->active_buckets) ||
	     !hlist_empty(&cur_fg->tw_pcbs))) {
		timer_add(&cur_fg->tcpip_timer, cur_fg,TCP_TMR_INTERVAL * ONE_MS);
	}
}


static inline void TCP_REG_ACTIVE(struct tcp_pcb *npcb, int idx, struct eth_fg *cur_fg)
{
	struct tcp_hash_entry *he = &cur_fg->active_tbl[idx];
	
	/* going from empty to non-empty list */
	if (hlist_empty(&he->pcbs))
		if (he->hash_link.prev == NULL)
			hlist_add_head(&cur_fg->active_buckets,&he->hash_link);
	
	TCP_REG(&he->pcbs, npcb,cur_fg);
	cur_fg->tcp_active_pcb_changed = 1;					
}


#define TCP_RMV_ACTIVE(npcb)                       \
  do {                                             \
    TCP_RMV(&perfg_get(tcp_active_pcbs), npcb);               \
    cur_fg->tcp_active_pcb_changed = 1;                   \
  } while (0)

#define TCP_PCB_REMOVE_ACTIVE(pcb)                 \
  do {                                             \
	  tcp_pcb_remove(cur_fg,pcb);			  \
    cur_fg->tcp_active_pcb_changed = 1;                   \
  } while (0)


/* Internal functions: */
struct tcp_pcb *tcp_pcb_copy(struct tcp_pcb *pcb);
void tcp_pcb_purge(struct tcp_pcb *pcb);
void tcp_pcb_remove(struct eth_fg *cur_fg,struct tcp_pcb *pcb);

void tcp_segs_free(struct tcp_seg *seg);
void tcp_seg_free(struct tcp_seg *seg);
struct tcp_seg *tcp_seg_copy(struct tcp_seg *seg);


static inline void
tcp_recompute_timers(struct eth_fg *cur_fg,struct tcp_pcb *pcb)
{
	uint64_t first = -1ul;
	if (pcb->timer_delayedack_expires>0)
		first = pcb->timer_delayedack_expires;
	if (pcb->timer_retransmit_expires>0 && pcb->timer_retransmit_expires<first)
		first = pcb->timer_retransmit_expires;
	if (pcb->timer_persist_expires>0 && pcb->timer_persist_expires<first)
		first = pcb->timer_persist_expires;

	if (timer_pending(&pcb->unified_timer) && first>=pcb->unified_timer.expires) {
		;/* nothing */
	} else {
		timer_del(&pcb->unified_timer);
		if (first != -1ul)
			timer_add_abs(&pcb->unified_timer,cur_fg,first);
	}
}

			
/* MAX_PACKETS_DELAYED_ACK -- LWIP behavior set to 2 */
#define MAX_PACKETS_DELAYED_ACK 2 
static inline void tcp_ack(struct eth_fg *cur_fg,struct tcp_pcb *pcb)
{
	if(pcb->timer_delayedack_expires>0) {	
		pcb->delayed_ack_counter++;
		if (pcb->delayed_ack_counter>=MAX_PACKETS_DELAYED_ACK) {
			pcb->timer_delayedack_expires = 0;
			tcp_recompute_timers(cur_fg,pcb);
			pcb->flags |= TF_ACK_NOW;
		}
	} else if (MAX_PACKETS_DELAYED_ACK == 1) {
		pcb->flags |= TF_ACK_NOW;
	} else {
		pcb->delayed_ack_counter = 1;
		pcb->timer_delayedack_expires = timer_now() + TCP_ACK_DELAY;
		tcp_recompute_timers(cur_fg,
pcb);
	}
}  
    
#define tcp_ack_now(pcb)                           \
  do {                                             \
    (pcb)->flags |= TF_ACK_NOW;                    \
  } while (0)

err_t tcp_send_fin(struct tcp_pcb *pcb);
err_t tcp_enqueue_flags(struct tcp_pcb *pcb, u8_t flags);

void tcp_rexmit_seg(struct tcp_pcb *pcb, struct tcp_seg *seg);

void tcp_rst_impl(struct eth_fg *cur_fg,u32_t seqno, u32_t ackno,
       ipX_addr_t *local_ip, ipX_addr_t *remote_ip,
       u16_t local_port, u16_t remote_port
#if LWIP_IPV6
       , u8_t isipv6
#endif /* LWIP_IPV6 */
       );
#if LWIP_IPV6
#define tcp_rst(seqno, ackno, local_ip, remote_ip, local_port, remote_port, isipv6) \
  tcp_rst_impl(seqno, ackno, local_ip, remote_ip, local_port, remote_port, isipv6)
#else /* LWIP_IPV6 */
#define tcp_rst(seqno, ackno, local_ip, remote_ip, local_port, remote_port, isipv6) \
	tcp_rst_impl(cur_fg,seqno, ackno, local_ip, remote_ip, local_port, remote_port)
#endif /* LWIP_IPV6 */

	u32_t tcp_next_iss(struct eth_fg *);

void tcp_keepalive(struct eth_fg *,struct tcp_pcb *pcb);
void tcp_zero_window_probe(struct eth_fg *,struct tcp_pcb *pcb);

#if TCP_CALCULATE_EFF_SEND_MSS
u16_t tcp_eff_send_mss_impl(u16_t sendmss, ipX_addr_t *dest
#if LWIP_IPV6
                           , ipX_addr_t *src, u8_t isipv6
#endif /* LWIP_IPV6 */
                           );
#if LWIP_IPV6
#define tcp_eff_send_mss(sendmss, src, dest, isipv6) tcp_eff_send_mss_impl(sendmss, dest, src, isipv6)
#else /* LWIP_IPV6 */
#define tcp_eff_send_mss(sendmss, src, dest, isipv6) tcp_eff_send_mss_impl(sendmss, dest)
#endif /* LWIP_IPV6 */
#endif /* TCP_CALCULATE_EFF_SEND_MSS */

#if LWIP_CALLBACK_API
err_t tcp_recv_null(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err);
#endif /* LWIP_CALLBACK_API */

#if TCP_DEBUG || TCP_INPUT_DEBUG || TCP_OUTPUT_DEBUG
void tcp_debug_print(struct tcp_hdr *tcphdr);
void tcp_debug_print_flags(u8_t flags);
void tcp_debug_print_state(enum tcp_state s);
void tcp_debug_print_pcbs(void);
s16_t tcp_pcbs_sane(void);
#else
#  define tcp_debug_print(tcphdr)
#  define tcp_debug_print_flags(flags)
#  define tcp_debug_print_state(s)
#  define tcp_debug_print_pcbs()
#  define tcp_pcbs_sane() 1
#endif /* TCP_DEBUG */

/** External function (implemented in timers.c), called when TCP detects
 * that a timer is needed (i.e. active- or time-wait-pcb found). */
void tcp_timer_needed(struct eth_fg *cur_fg);


#ifdef __cplusplus
}
#endif

#endif /* LWIP_TCP */

#endif /* __LWIP_TCP_H__ */
