/*
 * Mbuni - Open  Source MMS Gateway 
 * 
 * MMSC CFG: MMC configuration and misc. functions
 * 
 * Copyright (C) 2003 - 2008, Digital Solutions Ltd. - http://www.dsmagic.com
 *
 * Paul Bagyenda <bagyenda@dsmagic.com>
 * 
 * This program is free software, distributed under the terms of
 * the GNU General Public License, with a few exceptions granted (see LICENSE)
 */
#ifndef __MMSC_CFG__
#define __MMSC_CFG__
#include "mms_util.h"
#include "mms_resolve_shell.h"
#include "mms_billing_shell.h"
#include "mms_detokenize_shell.h"
#include "mms_queue.h"

typedef struct MmsProxyRelay {
     Octstr *host;
     Octstr *name;  
     Octstr *allowed_prefix;
     Octstr  *denied_prefix;

     Octstr *unified_prefix;
     List *strip_prefixes;
     
     int confirmed_mm4;
     Octstr *sendmail;
} MmsProxyRelay;

typedef struct MmsVasp {
     Octstr *id;
     long short_code;
     enum {SOAP_VASP, EAIF_VASP, NONE_VASP} type;
     Octstr *vasp_username, *vasp_password;
     Octstr *vasp_url;
     enum {UAProf_None, UAProf_URL, UAProf_UA} send_uaprof;
     MM7Version_t ver;
} MmsVasp;

typedef struct MmscSettings {
     Octstr *system_user;
     Octstr *name, *hostname, *host_alias;
     Octstr *unified_prefix, *local_prefix;

     List *strip_prefixes;
     
     Octstr *sendmail;
     
     Octstr *global_queuedir, *mm1_queuedir;
     Octstr *mmbox_rootdir;
     
     MmsQueueHandlerFuncs *qfs;

     Octstr *ua_profile_cache_dir;
     
     long maxthreads;
     long maxsendattempts;
     long default_msgexpiry;
     double queue_interval;
     long send_back_off;
     
     long port, mm7port;

     Octstr *allow_ip;
     Octstr *deny_ip;

     Octstr *email2mmsrelay_hosts; 
     Octstr *sendsms_url;
#if 0
     Octstr *sendsms_user, *sendsms_pass, *sendsms_globalsender;
#endif
     Octstr *billing_params;

     MmsBillingFuncStruct *mms_billfuncs; /* Link to billing funcs. */
     void *mms_bill_module_data;

     Octstr *resolver_params;
     MmsResolverFuncStruct *mms_resolvefuncs; /* Link to resolver funcs. */
     void *mms_resolver_module_data;

     Octstr *detokenizer_params;
     MmsDetokenizerFuncStruct *mms_detokenizefuncs; /* Link to detokenizer funcs. */
     void *mms_detokenizer_module_data;

     int allow_ip_type;

     int optimize_notification_size;
     int content_adaptation;
     int dlr_on_fetch;

     Octstr *prov_notify;

     Octstr *prov_getstatus;
     int notify_unprovisioned;
     Octstr *mms_notify_txt;
     Octstr *mms_notify_unprov_txt;
     Octstr *mms_toolarge;

     Octstr *mms_email_txt;
     Octstr *mms_email_html;
     Octstr *mms_email_subject;
     Octstr *wap_gw_msisdn_header;
     Octstr *wap_gw_ip_header;

     List *vasp_list; /* of MmsVasp * */

     MmsVasp *mms2email, *mms2mobile;
     
} MmscSettings;

/* Returns mmsc settings. */
MmscSettings *mms_load_mmsc_settings(mCfg *cfg, List **proxyrelays);

/* do final cleanup. */
void mms_cleanup_mmsc_settings(MmscSettings *settings);
/* Returns list of MmsProxyRelay */
extern List *mms_proxy_relays(mCfg *cfg, Octstr *myhostname);

extern Octstr *mms_makefetchurl(char *qf, Octstr *token, int loc,
				Octstr *to,
				MmscSettings *settings);

Octstr *mms_find_sender_msisdn(Octstr *send_url, 
			       Octstr *ip,
			       List *request_hdrs, 
			       Octstr *msisdn_header, 
			       Octstr *requestip_header,
			       MmsDetokenizerFuncStruct *detokenizerfuncs);

extern int mms_decodefetchurl(Octstr *fetch_url, 
			      Octstr **qf, Octstr **token, int *loc);
Octstr *mms_find_sender_ip(List *request_hdrs, Octstr *ip_header, Octstr *ip, int *isv6);

void notify_prov_server(char *cmd, char *from, char *event, char *arg, Octstr *msgid, 
			Octstr *ua, Octstr *uaprof);
int mms_ind_send(Octstr *prov_cmd, Octstr *to);

#endif
