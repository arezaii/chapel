/*   $Source: bitbucket.org:berkeleylab/gasnet.git/ucx-conduit/gasnet_core_sndrcv.c $
 * Description: GASNet ucx conduit implementation, transport send/receive logic
 * Copyright 2019-2020, Mellanox Technologies, Inc. All rights reserved.
 * Terms of use are as specified in license.txt
 */

#include <gasnet_internal.h>
#include <gasnet_core_internal.h>
#include <gasnet_am.h>
#include <gasnet_ucx_req.h>
#include <gasnet_event_internal.h>

GASNETI_INLINE(gasnetc_ucx_progress)
int gasnetc_ucx_progress(void);

GASNETI_INLINE(gasnetc_am_req_reset)
void gasnetc_am_req_reset(gasnetc_am_req_t *am_req);

GASNETI_INLINE(gasnetc_req_free)
void gasnetc_req_free(void *req);

GASNETI_INLINE(gasnetc_rreq_release)
void gasnetc_rreq_release(gasnetc_ucx_request_t *req);

size_t gasnetc_ammed_bufsz = (size_t)(-1); // "goes boom" if not overwritten

/* ------------------------------------------------------------------------------------ */
/*
  File-scoped completion callbacks
  ================================
*/
extern int gasnetc_complete_eop(gasnete_eop_t *eop, gasnetc_comptype_t type)
{ // Advance and test the proper counter
  gasnete_op_t *op = (gasnete_op_t*)eop;
  gasnetc_atomic_val_t completed;
  gasnetc_atomic_val_t initiated;

  switch (type) {
    case gasnetc_comptype_eop_alc:
      completed = gasnetc_atomic_add(&eop->completed_alc, 1, GASNETI_ATOMIC_ACQ);
      initiated = eop->initiated_alc;
      break;
    case gasnetc_comptype_eop_put:
      completed = gasnetc_atomic_add(&eop->completed_cnt, 1, GASNETI_ATOMIC_ACQ);
      initiated = eop->initiated_cnt;
      break;
    case gasnetc_comptype_eop_get:
      completed = gasnetc_atomic_add(&eop->completed_cnt, 1, GASNETI_ATOMIC_ACQ | GASNETI_ATOMIC_REL);
      initiated = eop->initiated_cnt;
      break;
    default:
      gasneti_unreachable();
  }

  if (completed == (initiated & GASNETI_ATOMIC_MAX)) {
    switch (type) {
      case gasnetc_comptype_eop_alc:
        GASNETE_EOP_LC_FINISH(op);
        break;
      case gasnetc_comptype_eop_put:
      case gasnetc_comptype_eop_get:
        GASNETE_EOP_MARKDONE(op);
        break;
      default:
        gasneti_unreachable();
    }
    return 1;
  }
  return 0;
}
// EOP completion callbacks
extern void gasnetc_cb_eop_alc(gasnetc_atomic_val_t *p) {
  gasnete_eop_t *eop = gasneti_container_of(p, gasnete_eop_t, initiated_alc);
  gasnete_eop_check(eop);
  (void) gasnetc_complete_eop(eop, gasnetc_comptype_eop_alc);
}
extern void gasnetc_cb_eop_put(gasnetc_atomic_val_t *p) {
  gasnete_eop_t *eop = gasneti_container_of(p, gasnete_eop_t, initiated_cnt);
  gasnete_eop_check(eop);
  (void) gasnetc_complete_eop(eop, gasnetc_comptype_eop_put);
}
extern void gasnetc_cb_eop_get(gasnetc_atomic_val_t *p) {
  gasnete_eop_t *eop = gasneti_container_of(p, gasnete_eop_t, initiated_cnt);
  gasnete_eop_check(eop);
  (void) gasnetc_complete_eop(eop, gasnetc_comptype_eop_get);
}

// NAR (nbi-accessregion) completion callbacks
extern void gasnetc_cb_nar_alc(gasnetc_atomic_val_t *p) {
  gasnete_iop_t *iop = gasneti_container_of(p, gasnete_iop_t, initiated_alc_cnt);
  gasnete_iop_check(iop);
  GASNETE_IOP_CNT_FINISH_REG(iop, alc, 1, GASNETI_ATOMIC_NONE);
}
extern void gasnetc_cb_nar_put(gasnetc_atomic_val_t *p) {
  gasnete_iop_t *iop = gasneti_container_of(p, gasnete_iop_t, initiated_put_cnt);
  gasnete_iop_check(iop);
  GASNETE_IOP_CNT_FINISH_REG(iop, put, 1, GASNETI_ATOMIC_NONE);
}
extern void gasnetc_cb_nar_get(gasnetc_atomic_val_t *p) {
  gasnete_iop_t *iop = gasneti_container_of(p, gasnete_iop_t, initiated_get_cnt);
  gasnete_iop_check(iop);
  GASNETE_IOP_CNT_FINISH_REG(iop, get, 1, GASNETI_ATOMIC_REL);
}
extern void gasnetc_cb_nar_rmw(gasnetc_atomic_val_t *p) {
  gasnete_iop_t *iop = gasneti_container_of(p, gasnete_iop_t, initiated_rmw_cnt);
  gasnete_iop_check(iop);
  GASNETE_IOP_CNT_FINISH_REG(iop, rmw, 1, GASNETI_ATOMIC_NONE);
}

// IOP (non accessregion) completion callbacks
extern void gasnetc_cb_iop_alc(gasnetc_atomic_val_t *p) {
  gasnete_iop_t *iop = gasneti_container_of(p, gasnete_iop_t, initiated_alc_cnt);
  gasnete_iop_check(iop);
  GASNETE_IOP_CNT_FINISH_INT(iop, alc, 1, GASNETI_ATOMIC_NONE);
}
extern void gasnetc_cb_iop_put(gasnetc_atomic_val_t *p) {
  gasnete_iop_t *iop = gasneti_container_of(p, gasnete_iop_t, initiated_put_cnt);
  gasnete_iop_check(iop);
  GASNETE_IOP_CNT_FINISH_INT(iop, put, 1, GASNETI_ATOMIC_NONE);
}
extern void gasnetc_cb_iop_get(gasnetc_atomic_val_t *p) {
  gasnete_iop_t *iop = gasneti_container_of(p, gasnete_iop_t, initiated_get_cnt);
  gasnete_iop_check(iop);
  GASNETE_IOP_CNT_FINISH_INT(iop, get, 1, GASNETI_ATOMIC_REL);
}
extern void gasnetc_cb_iop_rmw(gasnetc_atomic_val_t *p) {
  gasnete_iop_t *iop = gasneti_container_of(p, gasnete_iop_t, initiated_rmw_cnt);
  gasnete_iop_check(iop);
  GASNETE_IOP_CNT_FINISH_INT(iop, rmw, 1, GASNETI_ATOMIC_NONE);
}

// gasnetc_counter_t completion callbacks
extern void gasnetc_cb_counter(gasnetc_atomic_val_t *cnt) {
  gasnetc_counter_t *counter = gasneti_container_of(cnt, gasnetc_counter_t, initiated);
  gasnetc_atomic_increment(&counter->completed, 0);
}
extern void gasnetc_cb_counter_rel(gasnetc_atomic_val_t *cnt) {
  gasnetc_counter_t *counter = gasneti_container_of(cnt, gasnetc_counter_t, initiated);
  gasnetc_atomic_increment(&counter->completed, GASNETI_ATOMIC_REL);
}

extern void gasnetc_counter_wait(gasnetc_counter_t *counter,
                                 int outside_handler_context GASNETI_THREAD_FARG)
{
  const gasnetc_atomic_val_t initiated = (counter->initiated & GASNETI_ATOMIC_MAX);
  gasnetc_atomic_t * const completed = &counter->completed;

  if_pf (!gasnetc_counter_done(counter)) {
    if (outside_handler_context) {
      GASNETI_SPIN_DOUNTIL(initiated == gasnetc_atomic_read(completed, 0),
                           gasnetc_poll_sndrcv(GASNETC_LOCK_REGULAR GASNETI_THREAD_PASS));
    } else {
      // must not poll rcv queue in hander context
      GASNETI_SPIN_DOUNTIL(initiated == gasnetc_atomic_read(completed, 0),
                           gasnetc_poll_snd(GASNETC_LOCK_REGULAR GASNETI_THREAD_PASS));
    }
  }
}
/* ------------------------------------------------------------------------------------ */
/*
  Req/Mem pool functions
  ======================
*/

// May AM Long logic assume in-order delivery?
static int gasnetc_am_in_order = 1;

#if GASNETI_STATS_OR_TRACE
// Accounting for extra send-side buffers for Reply
static size_t gasnetc_extra_reply_bufs = 0;
#endif

GASNETI_INLINE(gasnetc_sreq_alloc)
gasnetc_am_req_t *gasnetc_sreq_alloc(gasneti_list_t *list) {
  gasnetc_am_req_t *am_req;
  GASNETI_LIST_ITEM_ALLOC(am_req, gasnetc_am_req_t, gasnetc_am_req_reset);
  am_req->buffer.data = gasneti_malloc_aligned(GASNETI_MEDBUF_ALIGNMENT,
                                               gasnetc_ammed_bufsz);
  GASNETC_BUF_RESET(am_req->buffer);
#if !GASNETC_PIN_SEGMENT
  am_req->buffer.long_data_ptr = NULL;
#endif
  am_req->list = list;
  if (list) {
    gasneti_list_enq(am_req->list, am_req);
  }
  return am_req;
}

// TODO: bug 4334:
// Code below allocates distinct small objects for each individual send buffer
// and its associated gasnetc_am_req_t, without any attention to cache-line
// alignment or allocator overheads. This should be refactored to use large
// allocations that are divided into cache-line-aligned chunks.  Ideally the
// region holding buffers can also be preemptively registered with UCX to
// reduce time/space registration overheads and improve TLB utilization.
void gasnetc_send_init(void)
{
  gasneti_list_init(&gasneti_ucx_module.send_queue);

  // Pool for sending AM Requests
  gasneti_list_init(&gasneti_ucx_module.sreq_free_req);
  for (int i = 0; i < GASNETC_UCX_REQ_POOL_SIZE; i++) {
    (void) gasnetc_sreq_alloc(&gasneti_ucx_module.sreq_free_req);
  }

  // Pool for sending AM Replies
  gasneti_list_init(&gasneti_ucx_module.sreq_free_rep);
  for (int i = 0; i < GASNETC_UCX_REQ_POOL_SIZE; i++) {
    (void) gasnetc_sreq_alloc(&gasneti_ucx_module.sreq_free_rep);
  }

  { // AM Long logic may assume in-order if nbhrd contains all proc on this host.
    // TODO: if !one_nbrhd, might parse UCX_GASNET_TLS (or UCX_TLS) to check
    //       for a setting which excludes use of UCX shared memory transports.
    // See bug 4155 for the issue this helps us address
#if !GASNET_PSHM
    gasneti_assert_uint(gasneti_mysupernode.node_count ,==, 1);
#endif
    const int one_nbrhd = (gasneti_mysupernode.node_count == gasneti_myhost.node_count);
    gasnetc_am_in_order = gasneti_getenv_yesno_withdefault("GASNET_UCX_AM_ORDERED_TLS", one_nbrhd);
  }
}

void gasnetc_send_fini(void)
{
  gasnetc_am_req_t *am_req;
  gasnetc_ucx_request_t *request;

  /* wait and release queued send requests */
  while(NULL != (request = GASNETI_LIST_POP(
                   &gasneti_ucx_module.send_queue, gasnetc_ucx_request_t))){
    if (GASNETC_UCX_ACTIVE == request->status) {
      ucp_request_cancel(gasneti_ucx_module.ucp_worker, request);
      while (GASNETC_UCX_ACTIVE == request->status) {
        gasnetc_ucx_progress();
        // TODO-next: interrupt if stuck here for a long time
      }
      gasneti_list_rem(&gasneti_ucx_module.send_queue, request);
      gasnetc_req_free(request);
    }
  }
  gasneti_list_fini(&gasneti_ucx_module.send_queue);

  /* release pools of send requests */
  while(NULL != (am_req = GASNETI_LIST_POP(
                   &gasneti_ucx_module.sreq_free_req, gasnetc_am_req_t))){
    gasneti_free_aligned(am_req->buffer.data);
    gasneti_free(am_req);
  }
  gasneti_list_fini(&gasneti_ucx_module.sreq_free_req);
  while(NULL != (am_req = GASNETI_LIST_POP(
                   &gasneti_ucx_module.sreq_free_rep, gasnetc_am_req_t))){
    gasneti_free_aligned(am_req->buffer.data);
    gasneti_free(am_req);
  }
  gasneti_list_fini(&gasneti_ucx_module.sreq_free_rep);
}

GASNETI_INLINE(gasnetc_am_req_get)
gasnetc_am_req_t *gasnetc_am_req_get(int is_request GASNETI_THREAD_FARG)
{
  gasnetc_am_req_t *am_req;

  gasnetc_ucx_progress();
  if (is_request) {
    GASNETI_SPIN_UNTIL((am_req = GASNETI_LIST_POP(&gasneti_ucx_module.sreq_free_req,
                                                  gasnetc_am_req_t)),
                       gasnetc_poll_sndrcv(GASNETC_LOCK_INLINE GASNETI_THREAD_PASS));
  } else {
    // Try at most twice (with a poll between) to allocate from the pool for reply buffers.
    // Excessive polling risks buffering additional UCX traffic, pushing us toward OOM.
    am_req = GASNETI_LIST_POP(&gasneti_ucx_module.sreq_free_rep, gasnetc_am_req_t);
    if (am_req) goto out;

    gasnetc_poll_snd(GASNETC_LOCK_INLINE GASNETI_THREAD_PASS);
    am_req = GASNETI_LIST_POP(&gasneti_ucx_module.sreq_free_rep, gasnetc_am_req_t);
    if_pt (am_req) goto out;

    // Next, try once to "borrow" from the request buffer pool
    // It will be returned to that pool upon completion
    am_req = GASNETI_LIST_POP(&gasneti_ucx_module.sreq_free_req, gasnetc_am_req_t);
    if_pt (am_req) {
      GASNETI_STAT_EVENT(C, BORROW_REPLY_BUF);
      goto out;
    }

    // Finally, dynamically allocate an extra one to be freed when completed
    // We need a gasnetc_am_req_t to send a reply, but have failed to find
    // one in the free pools, even after (multiple) attempt to progress ucx.
    // This likely inattentive peer(s) OR peers stuck in this same place!
    // Since we currently lack the necessary isolation to progress only the
    // reception of replies, that latter option spells deadlock if we spin
    // poll indefinitely.  Currently, the best option is to (temporarily)
    // grow the pool and thus buffer the outgoing reply.
    // Unfortunately, there is no a priori bound on this growth!
    // TODO: Bug 4359 for isolation and/or flow-control to avoid this mess.
  #if GASNETI_STATS_OR_TRACE
    gasnetc_extra_reply_bufs +=1;
    GASNETI_STAT_EVENT_VAL(C, EXTRA_REPLY_BUF, gasnetc_extra_reply_bufs);
  #endif
    // NULL list argument marks this allocation to be freed when complete
    am_req = gasnetc_sreq_alloc(NULL);
  }

out:
  return am_req;
}

GASNETI_INLINE(gasnetc_am_req_reset)
void gasnetc_am_req_reset(gasnetc_am_req_t *am_req)
{
  gasneti_assert(am_req && "Wrong req pointer");
  GASNETI_LIST_RESET(am_req);
#if !GASNETC_PIN_SEGMENT
  am_req->iov_count = 0;
#endif
  GASNETC_BUF_RESET(am_req->buffer);
  memset(&am_req->am_hdr, 0, sizeof(am_req->am_hdr));
}

GASNETI_INLINE(gasnetc_am_req_release)
void gasnetc_am_req_release(gasnetc_am_req_t *am_req)
{
  GASNETI_DBG_LIST_ITEM_CHECK(am_req);
#if !GASNETC_PIN_SEGMENT
  if (am_req->buffer.long_data_ptr) {
    gasneti_free(am_req->buffer.long_data_ptr);
    am_req->buffer.long_data_ptr = NULL;
  }
#endif
  if_pf (! am_req->list) { // allocated to meet temporary burst
  #if GASNETI_STATS_OR_TRACE
    gasnetc_extra_reply_bufs -=1;
  #endif
    gasneti_free_aligned(am_req->buffer.data);
    gasneti_free(am_req);
  } else {
    gasnetc_am_req_reset(am_req);
    gasneti_list_enq(am_req->list, am_req);
  }
}

/*
  Format send request
  ===================
*/
#if !GASNETC_PIN_SEGMENT
GASNETI_INLINE(gasnetc_req_add_iov)
void gasnetc_req_add_iov(gasnetc_am_req_t *am_req, void *buffer, size_t nbytes)
{
  gasneti_assert((am_req->iov_count !=  GASNETC_UCX_IOV_MAX)
                 && "Wrong iov count");
  gasneti_assert(nbytes);

  am_req->sendiov[am_req->iov_count].buffer = buffer;
  am_req->sendiov[am_req->iov_count].length = nbytes;
  am_req->iov_count++;
  GASNETC_AM_HDR_ADD_BYTES(am_req, nbytes);
}
#endif

GASNETI_INLINE(gasnetc_am_req_format)
void gasnetc_am_req_format(gasnetc_am_req_t *am_req,
                           gasnetc_ucx_am_type_t am_type,
                           gex_AM_Index_t handler, uint8_t is_packed,
                           uint8_t is_req, int numargs,
                           va_list argptr, uint32_t nbytes,
                           void *dst_addr GASNETI_THREAD_FARG)
{
  gasneti_assert(am_req);
  gasneti_assert(am_req->buffer.data);
  gasneti_assert(0 == GASNETC_BUF_SIZE(am_req->buffer));

  gasneti_static_assert(GASNETC_UCX_SHORT_HDR_SIZE(0) ==
                        offsetof(gasnetc_sreq_hdr_t, payload_size));
  gasneti_static_assert(GASNETC_UCX_MED_HDR_SIZE(0) ==
                        offsetof(gasnetc_sreq_hdr_t, dst_addr));
  gasneti_static_assert(GASNETC_UCX_LONG_HDR_SIZE(0) ==
                        sizeof(gasnetc_sreq_hdr_t));

  am_req->am_hdr = (gasnetc_sreq_hdr_t*)GASNETC_BUF_PTR(am_req->buffer);
  am_req->am_hdr->size = 0;

  am_req->am_hdr->am_type  = am_type;
  am_req->am_hdr->handler  = handler;
#if !GASNETC_PIN_SEGMENT
  am_req->am_hdr->is_packed = is_packed;
#endif
  am_req->am_hdr->is_req   = is_req;
  am_req->am_hdr->src      = gasneti_mynode;
  am_req->am_hdr->numargs  = numargs;

  size_t header_size;
  switch (am_type) {
    case GASNETC_UCX_AM_SHORT:
      header_size = GASNETC_UCX_SHORT_HDR_SIZE(0);
      break;
    case GASNETC_UCX_AM_MEDIUM: {
      am_req->am_hdr->payload_size = nbytes;
      header_size = GASNETC_UCX_MED_HDR_SIZE(0);
      break;
    }
    case GASNETC_UCX_AM_LONG: {
      am_req->am_hdr->payload_size = nbytes;
      am_req->am_hdr->dst_addr = dst_addr;
      header_size = GASNETC_UCX_LONG_HDR_SIZE(0);
      break;
    }
    default: gasneti_unreachable_error(("Invalid am_type in gasnetc_am_req_format"));
  }
  GASNETC_BUF_ADD_SEND_BYTES(am_req, header_size);

  am_req->args = (gex_AM_Arg_t*)GASNETC_BUF_PTR(am_req->buffer);
  GASNETC_BUF_ADD_SEND_BYTES(am_req, GASNETC_ARGS_SIZE(numargs));

  gasneti_assert(GASNETC_ARGS_SIZE(numargs) <= GASNETC_MAX_ARGS_SIZE);
  if (numargs) {
    for (int i = 0; i < numargs; i++) {
      am_req->args[i] = va_arg(argptr, gex_AM_Arg_t);
    }
  }
  if (GASNETC_UCX_AM_MEDIUM == am_type) {
    /* the payload following the arguments must be aligned
     * to GASNETI_MEDBUF_ALIGNMENT */
    unsigned int padding_size = GASNETC_AMMED_PADDING_SIZE(numargs);
    gasneti_assert_uint(padding_size ,<, GASNETI_MEDBUF_ALIGNMENT);
    GASNETC_BUF_ADD_SEND_BYTES(am_req, padding_size);
  }
}
/* ------------------------------------------------------------------------------------ */

/*
  RMA functions
  =============
*/
gasnetc_mem_info_t * gasnetc_find_mem_info(void *addr, int nbytes, gex_Rank_t rank)
{
  // TODO-future: use UCS rcache
  gasnetc_mem_info_t *mem_info;

  // TODO: thread safety in list traversal?
  GASNETI_LIST_FOREACH(mem_info, &gasneti_ucx_module.ep_tbl[rank].mem_tbl,
                       gasnetc_mem_info_t) {
    if (GASNETC_ADDR_IN_RANGE(mem_info->addr, mem_info->length, addr, nbytes)) {
      return mem_info;
    }
  }

  return NULL;
}

static
void gasnetc_ucx_rma_cb(void *request, ucs_status_t status)
{
  gasnetc_ucx_request_t *req = (gasnetc_ucx_request_t*) request;

  if (status != UCS_OK) {
    gasneti_fatalerror("UCX RDMA operation failed: %s",
                       ucs_status_string(status));
  }
  if (req->completion.cbfunc) {
    req->completion.cbfunc(req->completion.cbdata);
  }
  req->completion.cbfunc = NULL;
  req->completion.cbdata = NULL;
  ucp_request_free(request);
  return;
}

int gasnetc_ucx_putget_inner(int is_put, gex_Rank_t jobrank,
                             void *buffer, uint32_t nbytes, void *remote_addr,
                             gasnetc_atomic_val_t *local_cnt,
                             gasnetc_cbfunc_t local_cb,
                             gasnetc_atomic_val_t *remote_cnt,
                             gasnetc_cbfunc_t remote_cb)
{
  gasnetc_ucx_request_t *req;
  ucp_ep_h ep = GASNETC_UCX_GET_EP(jobrank);
  gasnetc_mem_info_t * minfo;
  int immediate = 0;

  minfo = gasnetc_find_mem_info(remote_addr, nbytes, jobrank);
  if (NULL == minfo) {
    gasneti_fatalerror("rkey cannot found");
  }
  if (local_cnt) (*local_cnt)++;
  
  req = gasnetc_putget_fn(is_put, ep, buffer, nbytes, remote_addr,
                          minfo->rkey, gasnetc_ucx_rma_cb);
  if (NULL == req) {
    /* completed inplace */
    if (local_cb) {
      local_cb(local_cnt);
    }
    immediate = 1;
  } else {
    if_pf (UCS_PTR_IS_ERR(req)) {
      gasneti_fatalerror("UCX RDMA put failed: %s",
                         ucs_status_string(UCS_PTR_STATUS(req)));
    }
    req->completion.cbdata = local_cnt;
    req->completion.cbfunc = local_cb;
  }

  if (remote_cnt) {
    (*remote_cnt)++;
    req = ucp_ep_flush_nb(ep, 0, gasnetc_ucx_rma_cb);
    if (req == NULL) {
      remote_cb(remote_cnt);
    } else {
      req->completion.cbdata = remote_cnt;
      req->completion.cbfunc = remote_cb;
    }
  }

  return immediate;
}

/* ------------------------------------------------------------------------------------ */
/*
  Send/recv requests
  ==================
*/
GASNETI_INLINE(gasnetc_req_reset)
void gasnetc_req_reset(void *request)
{
  gasnetc_ucx_request_t *req = (gasnetc_ucx_request_t *) request;
  req->status = GASNETC_UCX_INIT;
  req->am_req = NULL;
}

void gasnetc_req_init(void *request)
{
  GASNETI_DBG_LIST_ITEM_SET_MAGIC(request);
  gasnetc_req_reset(request);
}

GASNETI_INLINE(gasnetc_req_free)
void gasnetc_req_free(void *req)
{
  gasnetc_ucx_request_t *request = (gasnetc_ucx_request_t *) req;

  if (request->am_req) {
    gasnetc_am_req_release(request->am_req);
  }
  gasnetc_req_reset(request);
  ucp_request_free(request);
}

static void gasnetc_ucx_send_handler(void *request, ucs_status_t status)
{
  gasnetc_ucx_request_t *req = (gasnetc_ucx_request_t *)request;

  req->status = GASNETC_UCX_COMPLETE;
  if (UCS_ERR_CANCELED == status) {
    req->status = GASNETC_UCX_CANCEL;
    return;
  }
  if (req->completion.cbfunc) {
    req->completion.cbfunc(req->completion.cbdata);
  }
  req->completion.cbfunc = NULL;
  req->completion.cbdata = NULL;
exit:
  gasneti_list_rem(&gasneti_ucx_module.send_queue, req);
  gasnetc_req_free(req);
}

GASNETI_INLINE(gasnetc_send_req)
void gasnetc_send_req(gasnetc_am_req_t *am_req,
                                        gex_Rank_t jobrank,
                                        gasnetc_atomic_val_t *local_cnt,
                                        gasnetc_cbfunc_t local_cb)
{
  gasnetc_ucx_request_t *request = NULL;
  ucp_ep_h server_ep = gasneti_ucx_module.ep_tbl[jobrank].server_ep;
  void *src_ptr;
  size_t count;
  ucp_datatype_t datatype;

#if GASNETC_PIN_SEGMENT
  src_ptr = (void*)GASNETC_BUF_DATA(am_req->buffer);
  count = GASNETC_BUF_SIZE(am_req->buffer);
  datatype = ucp_dt_make_contig(1);
#else
  if_pt (!am_req->iov_count) {
    src_ptr = (void*)GASNETC_BUF_DATA(am_req->buffer);
    count = GASNETC_BUF_SIZE(am_req->buffer);
    datatype = ucp_dt_make_contig(1);
  } else {
    gasneti_assert((am_req->iov_count <= GASNETC_UCX_IOV_MAX)
                   && "Wrong iov count");
    src_ptr = (void*)am_req->sendiov;
    count = (size_t)am_req->iov_count;
    datatype = ucp_dt_make_iov();
  }
#endif

  if (local_cnt) (*local_cnt)++;

  request = ucp_tag_send_nb(server_ep, src_ptr, count, datatype,
      (ucp_tag_t)0, gasnetc_ucx_send_handler);
  if_pf (UCS_PTR_IS_ERR(request)) {
    gasnetc_am_req_release(am_req);
    gasneti_fatalerror("UCX recv request failed: %s",
                       ucs_status_string(UCS_PTR_STATUS(request)));
    /* Doesn't return */
  }
  if (NULL == request) {
      /* request was completed in place */
      if (local_cb) {
        local_cb(local_cnt);
      }
      gasnetc_am_req_release(am_req);
      goto exit;
  }

  request->am_req = am_req;
  request->completion.cbdata = local_cnt;
  request->completion.cbfunc = local_cb;
  request->status = GASNETC_UCX_ACTIVE;
  gasneti_list_enq(&gasneti_ucx_module.send_queue, request);

exit:
  return;
}
/* ------------------------------------------------------------------------------------ */
/*
  Active Message Request Functions
  ================================
*/
int gasnetc_am_reqrep_inner(gasnetc_ucx_am_type_t am_type,
           gex_Rank_t jobrank,
           gex_AM_Index_t handler,
           gex_Flags_t flags,
           uint8_t is_request,
           int numargs,
           va_list argptr,
           void *src_addr,
           uint32_t nbytes,
           void *dst_addr,
           gasnetc_atomic_val_t *local_cnt,
           gasnetc_cbfunc_t local_cb
           GASNETI_THREAD_FARG)
{
  gasnetc_am_req_t *am_req;

  GASNETC_LOCK_ACQUIRE(GASNETC_LOCK_REGULAR);
  am_req = gasnetc_am_req_get(is_request GASNETI_THREAD_PASS);
  gasneti_assert(am_req);

#define __am_req_format(__is_packed) \
  gasnetc_am_req_format(am_req, am_type, handler, __is_packed, \
                        is_request, numargs, argptr, nbytes, dst_addr \
                        GASNETI_THREAD_PASS)

  if (!nbytes) {
    /* GASNETC_UCX_AM_SHORT always goes this way */
    __am_req_format(0);
    goto send;
  }

  if (GASNETC_UCX_AM_MEDIUM == am_type ) {
      __am_req_format(0);
      gasneti_assert(src_addr);
      gasneti_assert_uint(nbytes ,<=, GASNETC_MAX_MED_(numargs));
      /* pack payload */
      GASNETI_MEMCPY(GASNETC_BUF_PTR(am_req->buffer), src_addr, nbytes);
      GASNETC_BUF_ADD_SEND_BYTES(am_req, nbytes);
  } else {
      gasneti_assert(GASNETC_UCX_AM_LONG == am_type);
      gasneti_assert(src_addr);
      gasneti_assert(dst_addr);
#if GASNETC_PIN_SEGMENT
      // TODO: packed Long
      // TODO: locality check would permit in-order assumption for off-node
      __am_req_format(0);
      if (gasnetc_am_in_order) {
        // Launch payload put with no stall prior to sending header
        gasnetc_ucx_putget_inner(1, jobrank, src_addr, nbytes, dst_addr,
            local_cnt, local_cb, NULL, NULL);
      } else {
        // Launch payload put and stall for remote completion prior to sending the header
        // See bug 4155 for the motivation
        gasnetc_counter_t rc_counter = GASNETC_COUNTER_INITIALIZER;
        gasnetc_ucx_putget_inner(1, jobrank, src_addr, nbytes, dst_addr,
            NULL, NULL, &rc_counter.initiated, gasnetc_cb_counter);
        GASNETC_LOCK_RELEASE(GASNETC_LOCK_REGULAR);
        gasnetc_counter_wait(&rc_counter, is_request GASNETI_THREAD_PASS);
        GASNETC_LOCK_ACQUIRE(GASNETC_LOCK_REGULAR);
      }
      // Do not delay LC for header send:
      local_cnt = NULL; local_cb = NULL;
#else // GASNETC_PIN_SEGMENT
      __am_req_format(1);
      if (nbytes <= GASNETC_MAX_MED_(numargs)) {
        GASNETI_MEMCPY(GASNETC_BUF_PTR(am_req->buffer), src_addr, nbytes);
        GASNETC_BUF_ADD_SEND_BYTES(am_req, nbytes);
      } else {
        gasnetc_req_add_iov(am_req, GASNETC_BUF_DATA(am_req->buffer),
                            GASNETC_BUF_SIZE(am_req->buffer));
        am_req->buffer.long_data_ptr = gasneti_malloc(nbytes);
        GASNETI_MEMCPY(am_req->buffer.long_data_ptr, src_addr, nbytes);
        am_req->buffer.long_bytes_used = nbytes;
        gasnetc_req_add_iov(am_req, am_req->buffer.long_data_ptr, nbytes);
      }
      gasneti_assume(local_cnt == NULL);
      gasneti_assume(local_cb == NULL);
#endif // GASNETC_PIN_SEGMENT
  }

send:
  // NOTE: local_cnt/local_cb here are NOT used for Longs.
  // Rather they provide LC stall for Short headers during shutdown
  gasnetc_send_req(am_req, jobrank, local_cnt, local_cb);
  GASNETC_LOCK_RELEASE(GASNETC_LOCK_REGULAR);

  return GASNET_OK;
}

int gasnetc_AM_ReqRepGeneric(gasnetc_ucx_am_type_t am_type,
                             gex_Rank_t jobrank,
                             gex_AM_Index_t handler,
                             gex_Event_t *lc_opt,
                             gex_Flags_t flags,
                             uint8_t is_request,
                             int numargs,
                             va_list argptr,
                             void *src_addr,
                             uint32_t nbytes,
                             void *dst_addr
                             GASNETI_THREAD_FARG)
{
  int retval;
  gasnete_eop_t *eop = NULL;
  gasnetc_counter_t *counter_ptr = NULL;
  gasnetc_cbfunc_t cbfunc = NULL;
  gasnetc_atomic_val_t *local_cnt = NULL;
  gasnetc_atomic_val_t start_cnt;
#if GASNETC_PIN_SEGMENT
  gasnetc_counter_t counter = GASNETC_COUNTER_INITIALIZER;
#endif

  if (GASNETC_UCX_AM_LONG == am_type) {
#if GASNETC_PIN_SEGMENT
    gasneti_threaddata_t * const mythread = GASNETI_MYTHREAD;

    if (gasneti_leaf_is_pointer(lc_opt)) {
      eop = gasnete_eop_new_alc(mythread);
      start_cnt = eop->initiated_alc;
      local_cnt = &eop->initiated_alc;
      cbfunc = gasnetc_cb_eop_alc;
      *lc_opt = (gex_Event_t) eop;
    } else if (lc_opt == GEX_EVENT_GROUP) {
      gasnete_iop_t *iop = mythread->current_iop;
      local_cnt = &iop->initiated_alc_cnt;
      cbfunc = iop->next ? gasnetc_cb_nar_alc : gasnetc_cb_iop_alc;
    } else {
      gasneti_assert(lc_opt == GEX_EVENT_NOW);
      local_cnt = &counter.initiated;
      cbfunc = gasnetc_cb_counter;
      counter_ptr = &counter;
    }
#else // GASNETC_PIN_SEGMENT
    gasneti_leaf_finish(lc_opt); // synchronous local completion
#endif
  }
  retval = gasnetc_am_reqrep_inner(am_type, jobrank, handler, flags, is_request,
                                   numargs, argptr, src_addr, nbytes, dst_addr,
                                   local_cnt, cbfunc
                                   GASNETI_THREAD_PASS);
  gasneti_assert(!retval); // at least until IMMEDIATE support is added

#if GASNETC_PIN_SEGMENT
  if (GASNETC_UCX_AM_LONG == am_type) {
    if (counter_ptr) {
      gasneti_assert_ptr(GEX_EVENT_NOW ,==, lc_opt);
      gasnetc_counter_wait(counter_ptr, is_request GASNETI_THREAD_PASS);
    } else if (eop) {
      gasneti_assume_leaf_is_pointer(lc_opt); // avoid maybe-uninitialized warning (like ibv bug 3756)
      if (start_cnt == eop->initiated_alc) {
        // Synchronous LC - reset LC state and pass-back INVALID_HANDLE
        GASNETE_EOP_LC_FINISH(eop);
        *lc_opt = GEX_EVENT_INVALID;
        gasnete_eop_free(eop GASNETI_THREAD_PASS);
      }
    }
  }
#endif

  return retval;
}

void gasnetc_ProcessRecv(void *buf, size_t size)
{
  gasnetc_sreq_hdr_t *am_hdr = (gasnetc_sreq_hdr_t*)buf;
  gex_AM_Index_t handler_id = am_hdr->handler;
  int numargs = am_hdr->numargs;
  int is_req = am_hdr->is_req;
  gasnetc_ucx_am_type_t am_type = am_hdr->am_type;
  const gex_AM_Fn_t handler_fn = gasnetc_handler[handler_id].gex_fnptr;
  gex_Token_t token_ptr = (gex_Token_t)am_hdr;

  switch(am_type) {
    case GASNETC_UCX_AM_SHORT: {
      size_t args_offset = GASNETC_UCX_SHORT_HDR_SIZE(0);
      gex_AM_Arg_t *args = (gex_AM_Arg_t*)((uintptr_t)buf + args_offset);
      gasneti_assert_uint(size ,==, args_offset + GASNETC_ARGS_SIZE(numargs));
      GASNETI_RUN_HANDLER_SHORT(is_req, handler_id, handler_fn, token_ptr,
                                args, numargs);
      break;
    }
    case GASNETC_UCX_AM_MEDIUM: {
      size_t args_offset = GASNETC_UCX_MED_HDR_SIZE(0);
      gex_AM_Arg_t *args = (gex_AM_Arg_t*)((uintptr_t)buf + args_offset);
      size_t header_size = GASNETC_UCX_MED_HDR_SIZE_PADDED(numargs);
      size_t nbytes = am_hdr->payload_size;
      gasneti_assert_uint(size ,==, header_size + nbytes);
      char *data = (char*)((uintptr_t)buf + header_size);
      GASNETI_RUN_HANDLER_MEDIUM(is_req, handler_id, handler_fn,
                                 token_ptr, args, numargs, data, nbytes);
      break;
    }
    case GASNETC_UCX_AM_LONG: {
      size_t args_offset = GASNETC_UCX_LONG_HDR_SIZE(0);
      gex_AM_Arg_t *args = (gex_AM_Arg_t*)((uintptr_t)buf + args_offset);
      size_t header_size = GASNETC_UCX_LONG_HDR_SIZE(numargs);
      size_t nbytes = am_hdr->payload_size;
#if GASNETC_PIN_SEGMENT
      gasneti_assert_uint(size ,==, header_size);
#else
      int is_packed = am_hdr->is_packed;
      gasneti_assert_uint(size ,==, header_size + (is_packed?nbytes:0));
      if (is_packed) {
        if (am_hdr->payload_size > 0) {
          gasneti_assert(am_hdr->dst_addr);
          char *data = (char*)((uintptr_t)buf + header_size);
          GASNETI_MEMCPY(am_hdr->dst_addr, data, nbytes);
        }
      }
#endif
      GASNETI_RUN_HANDLER_LONG(is_req, handler_id, handler_fn, token_ptr, args,
                               numargs, am_hdr->dst_addr, nbytes);
      break;
    }
  }
}

GASNETI_INLINE(gasnetc_ucx_progress)
int gasnetc_ucx_progress(void)
{
  int status;
  for(int i = 0;
      (status = ucp_worker_progress(gasneti_ucx_module.ucp_worker)) &&
        i < GASNETC_UCX_PROGRESS_CNT;
      i++);
  return status;
}

GASNETI_INLINE(gasnetc_recv_post)
void gasnetc_recv_post(gasnetc_ucx_request_t *req) {
  GASNETI_DBG_LIST_ITEM_CHECK(req);
  gasnetc_req_reset(req);
  req->ucs_status =
      ucp_tag_recv_nbr(gasneti_ucx_module.ucp_worker, req->buffer.data,
                       gasnetc_ammed_bufsz, ucp_dt_make_contig(1), 0, 0,
                       req);
  if_pf (req->ucs_status < 0) {
    gasneti_fatalerror("UCX post request failed: %s",
                       ucs_status_string(UCS_PTR_STATUS(req->ucs_status)));
  }
}

#if GASNETC_PIN_SEGMENT
int gasnetc_recv_init(void)
{
  int i;
  gasnetc_ucx_request_t *req;
  ucp_context_attr_t attr;
  ucs_status_t status;

  gasneti_list_init(&gasneti_ucx_module.recv_queue);

  attr.field_mask = UCP_ATTR_FIELD_REQUEST_SIZE;
  status = ucp_context_query(gasneti_ucx_module.ucp_context, &attr);
  gasneti_ucx_module.request_size = attr.request_size;

  // TODO: See comment preceding gasnetc_send_init regarding bug 4334
  for (i = 0; i < GASNETC_UCX_RCV_REAP_MAX; i++) {
    void *ucx_req = gasneti_malloc(gasneti_ucx_module.request_size +
                                   sizeof(gasnetc_ucx_request_t));
    req = (gasnetc_ucx_request_t*)
        (((char*) ucx_req) + gasneti_ucx_module.request_size);

    gasnetc_req_init(req);
    req->buffer.data = gasneti_malloc_aligned(GASNETI_MEDBUF_ALIGNMENT,
                                              gasnetc_ammed_bufsz);
    gasneti_list_enq(&gasneti_ucx_module.recv_queue, req);
    gasnetc_recv_post(req);
  }

  return GASNET_OK;
}

void gasnetc_recv_fini(void)
{
  gasnetc_ucx_request_t *req;
  void *ucx_req;

  while(NULL != (req = GASNETI_LIST_POP(&gasneti_ucx_module.recv_queue,
                                        gasnetc_ucx_request_t))) {
    ucx_req = ((char*) req) - gasneti_ucx_module.request_size;
    gasneti_free_aligned(req->buffer.data);
    gasneti_free(ucx_req);
  }
  gasneti_list_fini(&gasneti_ucx_module.recv_queue);
}

#else // GASNETC_PIN_SEGMENT
int gasnetc_recv_init(void)
{
  int i;
  gasnetc_am_req_t *rreq;

  gasneti_list_init(&gasneti_ucx_module.recv_queue);
  gasneti_list_init(&gasneti_ucx_module.rreq_free);

  // TODO: See comment preceding gasnetc_send_init regarding bug 4334
  for (i = 0; i < GASNETC_UCX_RCV_REAP_MAX; i++) {
    GASNETI_LIST_ITEM_ALLOC(rreq, gasnetc_am_req_t, gasnetc_am_req_reset);
    rreq->buffer.data = gasneti_malloc_aligned(GASNETI_MEDBUF_ALIGNMENT,
                                               gasnetc_ammed_bufsz);
    rreq->buffer.long_data_ptr = NULL;
    GASNETC_BUF_RESET(rreq->buffer);
    gasneti_list_enq(&gasneti_ucx_module.rreq_free, rreq);
  }

  return GASNET_OK;
}

void gasnetc_recv_fini(void)
{
  gasnetc_am_req_t *rreq;
  gasnetc_ucx_request_t *ucx_req;

  while (NULL != (ucx_req = GASNETI_LIST_POP(&gasneti_ucx_module.recv_queue,
                                             gasnetc_ucx_request_t))) {
    gasnetc_rreq_release(ucx_req);
  }
  gasneti_list_fini(&gasneti_ucx_module.recv_queue);

  while (NULL != (rreq = GASNETI_LIST_POP(&gasneti_ucx_module.rreq_free,
                                          gasnetc_am_req_t))) {
    gasneti_free_aligned(rreq->buffer.data);
#if !GASNETC_PIN_SEGMENT
    gasneti_assert(!rreq->buffer.long_bytes_used);
    gasneti_assert(!rreq->buffer.long_data_ptr);
#endif
    gasneti_free(rreq);
  }
  gasneti_list_fini(&gasneti_ucx_module.rreq_free);
}

#endif // GASNETC_PIN_SEGMENT

#if GASNETC_PIN_SEGMENT
int gasnetc_poll_sndrcv(gasnetc_lock_mode_t lmode GASNETI_THREAD_FARG)
{
  gasnetc_ucx_request_t *req, *tmp;
  gasnetc_sreq_hdr_t *am_hdr;

  GASNETC_LOCK_ACQUIRE(lmode);
  gasnetc_ucx_progress();

  req = gasneti_list_head(&gasneti_ucx_module.recv_queue);
  if (ucp_request_is_completed(req)) {
    tmp = GASNETI_LIST_POP(&gasneti_ucx_module.recv_queue,
                           gasnetc_ucx_request_t);
    gasneti_assume(tmp == req);
    am_hdr = (gasnetc_sreq_hdr_t*)req->buffer.data;
    GASNETC_BUF_SET_OFFSET(req->buffer, am_hdr->size);
    gasnetc_ProcessRecv(GASNETC_BUF_DATA(req->buffer),
                        GASNETC_BUF_SIZE(req->buffer));
    gasnetc_recv_post(req);
    gasneti_list_enq(&gasneti_ucx_module.recv_queue, req);
  }
  gasnetc_ucx_progress();
  GASNETC_LOCK_RELEASE(lmode);

#if GASNET_PSHM
  if (lmode == GASNETC_LOCK_REGULAR) {
    gasneti_AMPSHMPoll(0 GASNETI_THREAD_PASS);
  } else if (lmode == GASNETC_LOCK_INLINE) {
    /* `gasneti_AMPSHMPoll` should be called outside the lock */
    GASNETC_LOCK_RELEASE(GASNETC_LOCK_REGULAR);
    gasneti_AMPSHMPoll(0 GASNETI_THREAD_PASS);
    GASNETC_LOCK_ACQUIRE(GASNETC_LOCK_REGULAR);
  }
#endif
  return 0;
}

void gasnetc_poll_snd(gasnetc_lock_mode_t lmode GASNETI_THREAD_FARG)
{
  GASNETC_LOCK_ACQUIRE(lmode);
  gasnetc_ucx_progress();
  GASNETC_LOCK_RELEASE(lmode);
#if GASNET_PSHM
  if (lmode == GASNETC_LOCK_REGULAR) {
    gasneti_AMPSHMPoll(1 GASNETI_THREAD_PASS);
  } else if (lmode == GASNETC_LOCK_INLINE) {
    /* `gasneti_AMPSHMPoll` should be called outside the lock */
    GASNETC_LOCK_RELEASE(GASNETC_LOCK_REGULAR);
    gasneti_AMPSHMPoll(1 GASNETI_THREAD_PASS);
    GASNETC_LOCK_ACQUIRE(GASNETC_LOCK_REGULAR);
  }
#endif
}

#else // GASNETC_PIN_SEGMENT
static void gasneti_ucx_recv_handler(void *request, ucs_status_t status,
                                     ucp_tag_recv_info_t *info)
{
  gasnetc_ucx_request_t *req = (gasnetc_ucx_request_t*) request;

  if (UCS_ERR_CANCELED == status) {
    req->status = GASNETC_UCX_CANCEL;
    return;
  }
  if (status != UCS_OK) {
    req->status = GASNETC_UCX_FAILED;
    return;
  }
  if (req->status != GASNETC_UCX_INIT) {
    // enqueue the complete request to process in gasnetc_poll_sndrcv()
    gasneti_assert(req->am_req &&
                   ((info->length == GASNETC_BUF_SIZE(req->am_req->buffer)) ||
                    (info->length == GASNETC_BUF_LSIZE(req->am_req->buffer))));
    gasneti_list_enq(&gasneti_ucx_module.recv_queue, req);
  } else {
    // The request was completed synchronously and cannot be enqueued
    // here due to lack of the am_req field.
    // So, it is enqueued by gasnetc_poll_snd instead
  }
  req->status = GASNETC_UCX_COMPLETE;
}

// Despite the name, this function also advances two-stage AM reception
// However, it does not run any AM handlers
void gasnetc_poll_snd(gasnetc_lock_mode_t lmode GASNETI_THREAD_FARG)
{
  uint32_t probe_cnt = 0, probe_max;
  gasnetc_ucx_request_t *request = NULL;
  void *buf_ptr = NULL;
  ucp_tag_recv_info_t info_tag;
  ucp_tag_message_h msg_tag;
  gasnetc_am_req_t *rreq;

  GASNETC_LOCK_ACQUIRE(lmode);

  // Must progress at least once, even if rreq_free is empty,
  // in order to advance sends
  gasnetc_ucx_progress();

  // Drain up to GASNETC_UCX_RCV_REAP_MAX incoming receives (but not more than
  // available entries in rreq_free), posting the buffers needed to complete
  // reception.
  probe_max = gasneti_list_size(&gasneti_ucx_module.rreq_free);
  probe_max = MIN(probe_max, GASNETC_UCX_RCV_REAP_MAX);
  while((probe_cnt++) < probe_max) {
    gasnetc_ucx_progress();
    // check for new messages
    msg_tag = ucp_tag_probe_nb(gasneti_ucx_module.ucp_worker, 0, 0, 1, &info_tag);
    if (NULL == msg_tag) {
      break;
    }

    // allocate/initialize an gasnetc_am_req_t for asynchronous message reception
    rreq = GASNETI_LIST_POP(&gasneti_ucx_module.rreq_free, gasnetc_am_req_t);
    if (info_tag.length > gasnetc_ammed_bufsz) {
      // TODO/TBD: can/should tag bits pass the header length to allow use
      // of a 2-element iovec for reception, splitting the header from the
      // payload?
      rreq->buffer.long_data_ptr = gasneti_malloc(info_tag.length);
      rreq->buffer.long_bytes_used = info_tag.length;
      buf_ptr = rreq->buffer.long_data_ptr;
    } else {
      // TODO/TBD: can this case be handled w/ pre-posted receives as in
      // the GASNETC_PIN_SEGMENT case, using a tag bit to separate the
      // two classes of AM reception?
      rreq->buffer.bytes_used = info_tag.length;
      buf_ptr = rreq->buffer.data;
    }

    // "match" the new message, allowing UCX to complete reception
    request = (gasnetc_ucx_request_t*)
        ucp_tag_msg_recv_nb(gasneti_ucx_module.ucp_worker, buf_ptr,
                            info_tag.length, ucp_dt_make_contig(1), msg_tag,
                            gasneti_ucx_recv_handler);
    if (UCS_PTR_IS_ERR(request)) {
      gasneti_fatalerror("UCX recv request failed: %s",
                         ucs_status_string(UCS_PTR_STATUS(request)));
      /* gasneti_fatalerror doesn't return */
    }

    // link rreq from the recv_nb request
    request->am_req = rreq;

    if (GASNETC_UCX_COMPLETE == request->status) {
      // The request was completed synchronously and therefore
      // was not added to the recv_queue by gasneti_ucx_recv_handler().
      // So enqueue the complete request to process in gasnetc_poll_sndrcv()
      gasneti_list_enq(&gasneti_ucx_module.recv_queue, request);
    } else {
      request->status = GASNETC_UCX_ACTIVE;
    }
  }
  // TODO: trace/stats for probe_cnt?

  GASNETC_LOCK_RELEASE(lmode);
}

// Dequeue a completed request
GASNETI_INLINE(gasneti_req_probe_complete)
gasnetc_ucx_request_t *gasneti_req_probe_complete(gasneti_list_t *req_list)
{
  gasnetc_ucx_request_t *req;
  req = (gasnetc_ucx_request_t*)gasneti_list_deq(req_list);
  if (NULL != req) {
    gasneti_assert(GASNETC_UCX_COMPLETE == req->status);
    gasneti_assert(req->am_req->buffer.bytes_used ||
                   req->am_req->buffer.long_bytes_used);
  }
  return req;
}

GASNETI_INLINE(gasnetc_rreq_release)
void gasnetc_rreq_release(gasnetc_ucx_request_t *req)
{
  gasnetc_am_req_t *am_req = req->am_req;
  if (GASNETC_BUF_LSIZE(am_req->buffer)) {
    gasneti_free(GASNETC_BUF_LDATA(am_req->buffer));
    GASNETC_BUF_LDATA(am_req->buffer) = NULL;
  }
  GASNETC_BUF_RESET(am_req->buffer);
  gasneti_list_enq(&gasneti_ucx_module.rreq_free, am_req);
  gasnetc_req_reset(req);
  ucp_request_release(req);
}

int gasnetc_poll_sndrcv(gasnetc_lock_mode_t lmode GASNETI_THREAD_FARG)
{
  int recv_list_size = 0;
  gasnetc_ucx_request_t *request = NULL;
  gasneti_list_t local_recv_list;

  GASNETC_LOCK_ACQUIRE(lmode);

  // progress UCX and move completed receives to the recv_queue
  gasnetc_poll_snd(GASNETC_LOCK_INLINE GASNETI_THREAD_PASS);
  gasnetc_ucx_progress();

  // With the lock held, dequeue a batch of up to GASNETC_UCX_MSG_HNDL_PER_POLL
  // completed receives for processing without the lock held
  recv_list_size = gasneti_list_size(&gasneti_ucx_module.recv_queue);
  if_pt (!recv_list_size) {
    goto exit;
  }
  gasneti_list_init(&local_recv_list);
  int num_recv = MIN(recv_list_size, GASNETC_UCX_MSG_HNDL_PER_POLL);
  for (int i = 0; i < num_recv; ++i) {
    request = gasneti_req_probe_complete(&gasneti_ucx_module.recv_queue);
    gasneti_assert(request);
    gasneti_list_enq(&local_recv_list, request);
  }

  GASNETC_LOCK_RELEASE(GASNETC_LOCK_REGULAR);

  // handle the batch of received messages
  GASNETI_LIST_FOREACH(request, &local_recv_list, gasnetc_ucx_request_t) {
    gasneti_assert(request->am_req->buffer.bytes_used ||
                   request->am_req->buffer.long_bytes_used);
    if_pt (GASNETC_BUF_SIZE(request->am_req->buffer)) {
      gasnetc_ProcessRecv(GASNETC_BUF_DATA(request->am_req->buffer),
                          GASNETC_BUF_SIZE(request->am_req->buffer));
    } else {
      gasnetc_ProcessRecv(GASNETC_BUF_LDATA(request->am_req->buffer),
                          GASNETC_BUF_LSIZE(request->am_req->buffer));
    }
  }

  GASNETC_LOCK_ACQUIRE(GASNETC_LOCK_REGULAR);
  // release the batch requests
  while(NULL !=
        (request = GASNETI_LIST_POP(&local_recv_list, gasnetc_ucx_request_t))) {
    gasnetc_rreq_release(request);
  }
  gasneti_list_fini(&local_recv_list);

exit:
  GASNETC_LOCK_RELEASE(lmode);
  return recv_list_size;
}

#endif // GASNETC_PIN_SEGMENT

void gasnetc_send_list_wait(gasnetc_lock_mode_t lmode GASNETI_THREAD_FARG)
{
  size_t send_size;
  GASNETI_SPIN_DOWHILE(send_size, {
    GASNETC_LOCK_ACQUIRE(lmode);
    gasnetc_poll_sndrcv(GASNETC_LOCK_INLINE GASNETI_THREAD_PASS);
    send_size = gasneti_list_size(&gasneti_ucx_module.send_queue);
    GASNETC_LOCK_RELEASE(lmode);
  });
}
/* ------------------------------------------------------------------------------------ */
