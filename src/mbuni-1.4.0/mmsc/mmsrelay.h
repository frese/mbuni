#ifndef __MMSRELAY_INCLUDED__
#define __MMSRELAY_INCLUDED__
/*
 * Mbuni - Open  Source MMS Gateway 
 * 
 * MMS Relay, implements message routing
 * 
 * Copyright (C) 2003 - 2008, Digital Solutions Ltd. - http://www.dsmagic.com
 *
 * Paul Bagyenda <bagyenda@dsmagic.com>
 * 
 * This program is free software, distributed under the terms of
 * the GNU General Public License, with a few exceptions granted (see LICENSE)
 */
#include <signal.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include "mms_queue.h"
#include "mms_uaprof.h"
#include "mmsc_cfg.h"
#include "mms_mm7soap.h"

extern void mbuni_global_queue_runner(int *stopflag);
extern void mbuni_mm1_queue_runner(int *stopflag);
extern  MmscSettings *settings;
extern List *proxyrelays;
#endif
