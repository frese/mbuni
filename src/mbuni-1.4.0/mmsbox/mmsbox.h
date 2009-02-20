/*
 * Mbuni - Open  Source MMS Gateway 

 * 
 * MMSBOX CFG: MMC configuration and misc. functions
 * 
 * Copyright (C) 2003 - 2008, Digital Solutions Ltd. - http://www.dsmagic.com
 *
 * Paul Bagyenda <bagyenda@dsmagic.com>
 * 
 * This program is free software, distributed under the terms of
 * the GNU General Public License, with a few exceptions granted (see LICENSE)
 */
#ifndef __MMSBOX_INCLUDED__
#define __MMSBOX_INCLUDED__
#include "mmsbox_cfg.h"

extern int rstop;
extern Counter *mms_recieved_counter, *mms_sent_counter;

void mms_dlr_url_put(Octstr *msgid, char *rtype, Octstr *mmc_gid, Octstr *dlr_url, Octstr *transid);
int mms_dlr_url_get(Octstr *msgid, char *rtype, Octstr *mmc_gid, Octstr **dlr_url, Octstr **transid);
void mms_dlr_url_remove(Octstr *msgid, char *rtype, Octstr *mmc_gid);
int mmsbox_send_report(Octstr *from, char *report_type,
		       Octstr *dlr_url, Octstr *status, 
		       Octstr *msgid, Octstr *mmc_id, Octstr *mmc_gid, 
		       Octstr *orig_transid, Octstr *uaprof, 
		       time_t uaprof_tstamp);
void mmsc_receive_func(MmscGrp *m);
void mmsbox_outgoing_queue_runner(int *rstop);
void mmsbox_response_queue_runner(int *rstop);
void clean_duplicate_list(int *rstop);

enum {
    STATUS_HTML = 0,
    STATUS_TEXT = 1,
    STATUS_WML = 2,
    STATUS_XML = 3
};

/* Just a convenience, should go away in future! */
#define mmsbox_url_fetch_content mms_url_fetch_content
#endif
