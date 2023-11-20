/*
 * Copyright (c) 2001-2003 Swedish Institute of Computer Science.
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

/*
 * Wed Apr 17 16:05:29 EDT 2002 (James Roth)
 *
 *  - Fixed an unlikely sys_thread_new() race condition.
 *
 *  - Made current_thread() work with threads which where
 *    not created with sys_thread_new().  This includes
 *    the main thread and threads made with pthread_create().
 *
 *  - Catch overflows where more than SYS_MBOX_SIZE messages
 *    are waiting to be read.  The sys_mbox_post() routine
 *    will block until there is more room instead of just
 *    leaking messages.
 */

#include "include/arch/sys_arch.h"

#include "lwip/sys.h"
#include "lwip/tcpip.h"

#if SYS_LIGHTWEIGHT_PROT
static GRecMutex lwprot_mutex;
#endif

#if !NO_SYS

static gint next_thread_id = 1;

struct sys_mbox {
  GAsyncQueue *queue;
};

struct sys_sem {
  guint count;
  GMutex mutex;
  GCond cond;
};

struct sys_mutex {
  GMutex mutex;
};

struct thread_wrapper_data {
  lwip_thread_fn function;
  void *arg;
};

static gpointer thread_wrapper(gpointer data);

sys_thread_t
sys_thread_new(const char *name, lwip_thread_fn function, void *arg, int stacksize, int prio)
{
  struct thread_wrapper_data *thread_data;

  LWIP_UNUSED_ARG(stacksize);
  LWIP_UNUSED_ARG(prio);

  thread_data = g_slice_new(struct thread_wrapper_data);
  thread_data->arg = arg;
  thread_data->function = function;

  g_thread_unref(g_thread_new(name, thread_wrapper, thread_data));

  return g_atomic_int_add(&next_thread_id, 1);
}

static gpointer
thread_wrapper(gpointer data)
{
  struct thread_wrapper_data *thread_data = data;

  thread_data->function(thread_data->arg);

  g_slice_free(struct thread_wrapper_data, data);

  return NULL;
}

#if LWIP_TCPIP_CORE_LOCKING

static GThread *lwip_core_lock_holder_thread;

void sys_lock_tcpip_core(void)
{
  sys_mutex_lock(&lock_tcpip_core);
  lwip_core_lock_holder_thread = g_thread_self();
}

void sys_unlock_tcpip_core(void)
{
  lwip_core_lock_holder_thread = 0;
  sys_mutex_unlock(&lock_tcpip_core);
}

#endif

static GThread *lwip_tcpip_thread;

void sys_mark_tcpip_thread(void)
{
  lwip_tcpip_thread = g_thread_self();
}

void sys_check_core_locking(void)
{
  if (lwip_tcpip_thread != NULL) {
    GThread *current_thread = g_thread_self();

#if LWIP_TCPIP_CORE_LOCKING
    LWIP_ASSERT("Function called without core lock", current_thread == lwip_core_lock_holder_thread);
#else /* LWIP_TCPIP_CORE_LOCKING */
    LWIP_ASSERT("Function called from wrong thread", current_thread == lwip_tcpip_thread_id);
#endif /* LWIP_TCPIP_CORE_LOCKING */
  }
}

err_t
sys_mbox_new(struct sys_mbox **mb, int size)
{
  struct sys_mbox *mbox;
  LWIP_UNUSED_ARG(size);

  mbox = g_slice_new(struct sys_mbox);
  mbox->queue = g_async_queue_new();

  SYS_STATS_INC_USED(mbox);
  *mb = mbox;

  return ERR_OK;
}

void
sys_mbox_free(struct sys_mbox **mb)
{
  if ((mb != NULL) && (*mb != SYS_MBOX_NULL)) {
    struct sys_mbox *mbox = *mb;
    SYS_STATS_DEC(mbox.used);

    g_async_queue_lock(mbox->queue);
    g_async_queue_unlock(mbox->queue);

    g_async_queue_unref(mbox->queue);
    g_slice_free(struct sys_mbox, mbox);
  }
}

err_t
sys_mbox_trypost(struct sys_mbox **mb, void *msg)
{
  struct sys_mbox *mbox;

  LWIP_ASSERT("invalid mbox", (mb != NULL) && (*mb != NULL));
  mbox = *mb;

  g_async_queue_push(mbox->queue, msg);

  return ERR_OK;
}

err_t
sys_mbox_trypost_fromisr(sys_mbox_t *q, void *msg)
{
  return sys_mbox_trypost(q, msg);
}

void
sys_mbox_post(struct sys_mbox **mb, void *msg)
{
  sys_mbox_trypost(mb, msg);
}

u32_t
sys_arch_mbox_tryfetch(struct sys_mbox **mb, void **msg)
{
  struct sys_mbox *mbox;
  void *m;

  LWIP_ASSERT("invalid mbox", (mb != NULL) && (*mb != NULL));
  mbox = *mb;

  m = g_async_queue_try_pop(mbox->queue);
  if (m == NULL)
    return SYS_MBOX_EMPTY;

  if (msg != NULL) {
    LWIP_DEBUGF(SYS_DEBUG, ("sys_mbox_tryfetch: mbox %p msg %p\n", (void *)mbox, *msg));
    *msg = m;
  }
  else{
    LWIP_DEBUGF(SYS_DEBUG, ("sys_mbox_tryfetch: mbox %p, null msg\n", (void *)mbox));
  }

  return 0;
}

u32_t
sys_arch_mbox_fetch(struct sys_mbox **mb, void **msg, u32_t timeout)
{
  struct sys_mbox *mbox;
  void *m;

  LWIP_ASSERT("invalid mbox", (mb != NULL) && (*mb != NULL));
  mbox = *mb;

  m = g_async_queue_timeout_pop(mbox->queue, timeout * 1000);
  if (m == NULL)
    return SYS_ARCH_TIMEOUT;

  if (msg != NULL) {
    LWIP_DEBUGF(SYS_DEBUG, ("sys_mbox_fetch: mbox %p msg %p\n", (void *)mbox, *msg));
    *msg = m;
  }
  else{
    LWIP_DEBUGF(SYS_DEBUG, ("sys_mbox_fetch: mbox %p, null msg\n", (void *)mbox));
  }

  return 0;
}

err_t
sys_sem_new(struct sys_sem **sem, u8_t count)
{
  struct sys_sem *s;

  s = g_slice_new(struct sys_sem);
  s->count = count;
  g_mutex_init(&s->mutex);
  g_cond_init(&s->cond);

  *sem = s;

  return ERR_OK;
}

u32_t
sys_arch_sem_wait(struct sys_sem **s, u32_t timeout)
{
  struct sys_sem *sem;
  gint64 end_time;

  LWIP_ASSERT("invalid sem", (s != NULL) && (*s != NULL));
  sem = *s;

  end_time = g_get_monotonic_time() + (timeout * G_TIME_SPAN_MILLISECOND);

  g_mutex_lock(&sem->mutex);

  while (sem->count == 0) {
    if (!g_cond_wait_until(&sem->cond, &sem->mutex, end_time)) {
      g_mutex_unlock(&sem->mutex);
      return SYS_ARCH_TIMEOUT;
    }
  }

  sem->count = 0;

  g_mutex_unlock(&sem->mutex);

  return 0;
}

void
sys_sem_signal(struct sys_sem **s)
{
  struct sys_sem *sem;

  LWIP_ASSERT("invalid sem", (s != NULL) && (*s != NULL));
  sem = *s;

  g_mutex_lock(&sem->mutex);

  sem->count = 1;

  g_cond_signal(&sem->cond);
  g_mutex_unlock(&sem->mutex);
}

void
sys_sem_free(struct sys_sem **sem)
{
  if ((sem != NULL) && (*sem != SYS_SEM_NULL)) {
    struct sys_sem *s = *sem;

    SYS_STATS_DEC(sem.used);

    g_cond_clear(&s->cond);
    g_mutex_clear(&s->mutex);
    g_slice_free(struct sys_sem, s);
  }
}

err_t
sys_mutex_new(struct sys_mutex **mutex)
{
  struct sys_mutex *mtx;

  mtx = g_slice_new(struct sys_mutex);
  g_mutex_init(&(mtx->mutex));

  *mutex = mtx;

  return ERR_OK;
}

void
sys_mutex_lock(struct sys_mutex **mutex)
{
  g_mutex_lock(&((*mutex)->mutex));
}

void
sys_mutex_unlock(struct sys_mutex **mutex)
{
  g_mutex_unlock(&((*mutex)->mutex));
}

void
sys_mutex_free(struct sys_mutex **mutex)
{
  g_mutex_clear(&((*mutex)->mutex));
  g_slice_free(struct sys_mutex, *mutex);
}

#endif /* !NO_SYS */

u32_t
sys_now(void)
{
  u32_t now;

  now = (u32_t)(g_get_monotonic_time() / G_GUINT64_CONSTANT(1000));
#ifdef LWIP_FUZZ_SYS_NOW
  now += sys_now_offset;
#endif
  return now;
}

u32_t
sys_jiffies(void)
{
  return g_get_monotonic_time() * G_GUINT64_CONSTANT(1000);
}

void
sys_init(void)
{
}

#if SYS_LIGHTWEIGHT_PROT

sys_prot_t
sys_arch_protect(void)
{
  g_rec_mutex_lock(&lwprot_mutex);
  return 0;
}

void
sys_arch_unprotect(sys_prot_t pval)
{
  g_rec_mutex_unlock(&lwprot_mutex);
}

#endif
