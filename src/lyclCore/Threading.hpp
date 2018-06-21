/*
 * Copyright 2010 Jeff Garzik
 * Copyright 2012 Luke Dashjr
 * Copyright 2012-2014 pooler
 * Copyright 2018 CryptoGraphics ( CrGraphics@protonmail.com )
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version. See LICENSE for more details.
 */

#ifndef Threading_INCLUDE_ONCE
#define Threading_INCLUDE_ONCE

#include <stdint.h>
#include <pthread.h> // pthread_mutex
#include <lyclCore/CLUtils.hpp>
#include <lyclCore/Elist.hpp>

struct tq_ent
{
    void *data;
    struct list_head q_node;
};

struct thread_q
{
    struct list_head q;

    bool frozen;

    pthread_mutex_t mutex;
    pthread_cond_t cond;
};

struct thr_info
{
    int id;
    pthread_t pth;
    pthread_attr_t attr;
    thread_q *q;
    lycl::device clDevice; 
};

inline static int thread_create(struct thr_info *thr, void *(*func) (void *))
{
    int err = 0;
    pthread_attr_init(&thr->attr);
    err = pthread_create(&thr->pth, &thr->attr, func, thr);
    pthread_attr_destroy(&thr->attr);
    return err;
}

static thread_q *tq_new()
{
    thread_q *tq;

    tq = (struct thread_q*) calloc(1, sizeof(*tq));
    if (!tq)
        return NULL;

    INIT_LIST_HEAD(&tq->q);
    pthread_mutex_init(&tq->mutex, NULL);
    pthread_cond_init(&tq->cond, NULL);

    return tq;
}

static bool tq_push(struct thread_q *tq, void *data)
{
    struct tq_ent *ent;
    bool rc = true;

    ent = (struct tq_ent*) calloc(1, sizeof(*ent));
    if (!ent)
        return false;

    ent->data = data;
    INIT_LIST_HEAD(&ent->q_node);

    pthread_mutex_lock(&tq->mutex);

    if (!tq->frozen) {
        list_add_tail(&ent->q_node, &tq->q);
    } else {
        free(ent);
        rc = false;
    }

    pthread_cond_signal(&tq->cond);
    pthread_mutex_unlock(&tq->mutex);

    return rc;
}

static void* tq_pop(struct thread_q *tq, const struct timespec *abstime)
{
    struct tq_ent *ent;
    void *rval = NULL;
    int rc;

    pthread_mutex_lock(&tq->mutex);

    if (!list_empty(&tq->q))
        goto pop;

    if (abstime)
        rc = pthread_cond_timedwait(&tq->cond, &tq->mutex, abstime);
    else
        rc = pthread_cond_wait(&tq->cond, &tq->mutex);
    if (rc)
        goto out;
    if (list_empty(&tq->q))
        goto out;

pop:
    ent = list_entry(tq->q.next, struct tq_ent, q_node);
    rval = ent->data;

    list_del(&ent->q_node);
    free(ent);

out:
    pthread_mutex_unlock(&tq->mutex);
    return rval;
}

static void tq_freezethaw(struct thread_q *tq, bool frozen)
{
    pthread_mutex_lock(&tq->mutex);

    tq->frozen = frozen;

    pthread_cond_signal(&tq->cond);
    pthread_mutex_unlock(&tq->mutex);
}

static void tq_freeze(struct thread_q *tq)
{
    tq_freezethaw(tq, true);
}

#endif // !Threading_INCLUDE_ONCE
