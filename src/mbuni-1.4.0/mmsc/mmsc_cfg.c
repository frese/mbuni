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
#include <sys/file.h>
#include <ctype.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <errno.h>
#include <dlfcn.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "mmsc_cfg.h"
#include "mms_queue.h"


#define MMS_PORT 8191  /* Default content fetch port. */



void mms_cleanup_mmsc_settings(MmscSettings *settings)
{
     /* eventually we will destroy the object. For now, we only cleanup queue module. */
     settings->qfs->mms_cleanup_queue_module();
}

MmscSettings *mms_load_mmsc_settings(mCfg *cfg, List **proxyrelays)
{
     Octstr *s;

     List *l;
     mCfgGrp *grp = mms_cfg_get_single(cfg, octstr_imm("mbuni"));
     mCfgGrp *cgrp = mms_cfg_get_single(cfg, octstr_imm("core"));
     MmscSettings *m = gw_malloc(sizeof *m);
     long port = -1, max_memory_queued;
     Octstr  *from,  *user, *pass;
     Octstr *qdir = NULL;
     int i, n, xx = 0;

     memset(m, 0, sizeof *m);

     if (grp == NULL)
	  panic(0,"Missing required group `mbuni' in config file!");

     mms_load_core_settings(cgrp);

     m->hostname = mms_cfg_get(grp, octstr_imm("hostname"));

     if (octstr_len(m->hostname) == 0)
	  m->hostname = octstr_create("localhost");
     
     if ((m->host_alias = mms_cfg_get(grp, octstr_imm("host-alias"))) == NULL)
	  m->host_alias = octstr_duplicate(m->hostname);
     
     if (proxyrelays)
	  *proxyrelays = mms_proxy_relays(cfg, m->hostname);
          
     if (mms_cfg_get_int(grp, octstr_imm("max-send-threads"), &m->maxthreads) == -1)
	  m->maxthreads = 10;
	mms_cfg_get_int(grp, octstr_imm("max-memory-queued"), &max_memory_queued);
     
     m->unified_prefix = _mms_cfg_getx(grp, octstr_imm("unified-prefix"));        
     m->local_prefix = _mms_cfg_getx(grp, octstr_imm("local-prefixes"));        
     
     if ((s = mms_cfg_get(grp, octstr_imm("strip-prefixes"))) != NULL) {
	  m->strip_prefixes = octstr_split(s, octstr_imm(";"));
	  octstr_destroy(s);
     } else 
	  m->strip_prefixes = NULL;
     

     m->name = _mms_cfg_getx(grp, octstr_imm("name"));

     m->sendmail = _mms_cfg_getx(grp, octstr_imm("send-mail-prog"));
     
     qdir = _mms_cfg_getx(grp, octstr_imm("storage-directory")); 
     if (qdir && octstr_len(qdir) >= QFNAMEMAX)
	  warning(0, "storage-directory name too long. Max length is %d", QFNAMEMAX);

     if (mkdir(octstr_get_cstr(qdir), 
	       S_IRWXU|S_IRWXG) < 0 && 
	 errno != EEXIST)
	  panic(0, "Failed to create queue directory: %s - %s!",
		octstr_get_cstr(qdir), strerror(errno));
		
     if ((m->qfs = _mms_load_module(grp, "queue-manager-module", "qfuncs", NULL)) == NULL) {
	  m->qfs = &default_qfuncs; /* default queue handler. */
	  m->qfs->mms_init_queue_module(qdir, m->maxthreads, max_memory_queued);
     } else {
	  Octstr *s = _mms_cfg_getx(grp, octstr_imm("queue-module-init-data"));
	  if (m->qfs->mms_init_queue_module(s, m->maxthreads, max_memory_queued) != 0)
	       panic(0, "failed to initialise queue module, with data: %s",
		     octstr_get_cstr(s));
	  octstr_destroy(s);
     }
     
     if ((m->global_queuedir = m->qfs->mms_init_queue_dir("global", &xx)) == NULL ||
	 xx != 0)
	  panic(0, "Failed to initialise global queue directory: %s - %s!", 
		octstr_get_cstr(m->global_queuedir), strerror(errno));

     if ((m->mm1_queuedir = m->qfs->mms_init_queue_dir("mm1", &xx)) == NULL ||
	 xx != 0)
	  panic(0, "Failed to initialise local queue directory: %s - %s!", 
		octstr_get_cstr(m->mm1_queuedir), strerror(errno));

     m->mmbox_rootdir = octstr_format("%S/mmbox", qdir);     
     if (mmbox_root_init(octstr_get_cstr(m->mmbox_rootdir)) != 0)
	  panic(0, "Failed to initialise mmbox root directory, error: %s!", 
		strerror(errno));
     
     m->ua_profile_cache_dir = octstr_format("%S/UserAgent_Profiles", qdir);
     
     if (mkdir(octstr_get_cstr(m->ua_profile_cache_dir), 
	       S_IRWXU|S_IRWXG) < 0 && 
	 errno != EEXIST)
	  panic(0, "Failed to initialise UA Profile directory, error: %s!",
		strerror(errno));

     if (mms_cfg_get_int(grp, octstr_imm("maximum-send-attempts"), &m->maxsendattempts) == -1)
	  m->maxsendattempts = MAXQTRIES;

     if (mms_cfg_get_int(grp, octstr_imm("default-message-expiry"), &m->default_msgexpiry) == -1)
	  m->default_msgexpiry = DEFAULT_EXPIRE;

     s = _mms_cfg_getx(grp, octstr_imm("queue-run-interval"));
     if (!s || (m->queue_interval = atof(octstr_get_cstr(s))) <= 0)
	  m->queue_interval = QUEUERUN_INTERVAL;


     octstr_destroy(s);
     
     if (mms_cfg_get_int(grp, octstr_imm("send-attempt-back-off"), &m->send_back_off) == -1)
	  m->send_back_off = BACKOFF_FACTOR;

     /* Make send sms url. */
     m->sendsms_url = _mms_cfg_getx(grp, octstr_imm("sendsms-url"));     
     
     user = mms_cfg_get(grp, octstr_imm("sendsms-username"));     
     pass = mms_cfg_get(grp, octstr_imm("sendsms-password"));     
     from = mms_cfg_get(grp, octstr_imm("sendsms-global-sender"));     
     
     i = octstr_search_char(m->sendsms_url, '?', 0); /* If ? is in there, omit below. */

     octstr_format_append(m->sendsms_url, 
			  (from && octstr_len(from) > 1) ? 
			  "%sfrom=%E" : "%s_dummy=x",	  
			  (i >= 0) ? "" : "?", from);	
   
     if (user && octstr_len(user) > 0)
	  octstr_format_append(m->sendsms_url,
			       "&username=%E&password=%E",
			       user, pass);
     m->system_user = octstr_format("system-user@%S", 
				    m->hostname);	  
     octstr_destroy(user);
     octstr_destroy(pass);
     octstr_destroy(from);
     
     mms_cfg_get_int(grp, octstr_imm("mms-port"), &port);

     m->port = (port > 0) ? port : MMS_PORT;

     m->mm7port = -1;
     mms_cfg_get_int(grp, octstr_imm("mm7-port"), &m->mm7port);

     m->allow_ip = _mms_cfg_getx(grp, octstr_imm("allow-ip"));          
     m->deny_ip = _mms_cfg_getx(grp, octstr_imm("deny-ip"));          

     m->email2mmsrelay_hosts = _mms_cfg_getx(grp, 
					   octstr_imm("email2mms-relay-hosts"));
     
     m->prov_notify = _mms_cfg_getx(grp,octstr_imm("prov-server-notify-script"));

     m->prov_getstatus = _mms_cfg_getx(grp,octstr_imm("prov-server-sub-status-script"));     
     m->mms_notify_txt = _mms_cfg_getx(grp, octstr_imm("mms-notify-text"));
     m->mms_notify_unprov_txt = _mms_cfg_getx(grp, octstr_imm("mms-notify-unprovisioned-text"));


     m->mms_email_txt = _mms_cfg_getx(grp, octstr_imm("mms-to-email-txt"));
     m->mms_email_html = _mms_cfg_getx(grp, octstr_imm("mms-to-email-html"));
     m->mms_email_subject = mms_cfg_get(grp, octstr_imm("mms-to-email-default-subject"));

     m->mms_toolarge = _mms_cfg_getx(grp, octstr_imm("mms-message-too-large-txt"));

     m->wap_gw_msisdn_header = mms_cfg_get(grp, octstr_imm("mms-client-msisdn-header"));
     if (!m->wap_gw_msisdn_header) m->wap_gw_msisdn_header = octstr_imm(XMSISDN_HEADER);

     m->wap_gw_ip_header = mms_cfg_get(grp, octstr_imm("mms-client-ip-header"));
     if (!m->wap_gw_ip_header) m->wap_gw_ip_header = octstr_imm(XIP_HEADER);

     mms_cfg_get_bool(grp, octstr_imm("notify-unprovisioned"), &m->notify_unprovisioned);

     m->billing_params = _mms_cfg_getx(grp, 
				  octstr_imm("billing-module-parameters"));
     /* Get and load the billing lib if any. */
     
     if ((m->mms_billfuncs = _mms_load_module(grp, "billing-library",  "mms_billfuncs", 
	       &mms_billfuncs_shell)) != NULL) {
	  if (m->mms_billfuncs->mms_billingmodule_init == NULL ||
	      m->mms_billfuncs->mms_billmsg == NULL ||
	      m->mms_billfuncs->mms_billingmodule_fini == NULL ||
	      m->mms_billfuncs->mms_logcdr == NULL) 
	       panic(0, "Missing or NULL functions in billing module!");
     } else 
	  m->mms_billfuncs = &mms_billfuncs; /* The default one. */	  
     
     m->mms_bill_module_data = m->mms_billfuncs->mms_billingmodule_init(octstr_get_cstr(m->billing_params));
     
     m->resolver_params = _mms_cfg_getx(grp, 
				   octstr_imm("resolver-module-parameters"));
     
     /* Get and load the resolver lib if any. */
     if ((m->mms_resolvefuncs = _mms_load_module(grp, "resolver-library",  
					    "mms_resolvefuncs", 
	       &mms_resolvefuncs_shell)) != NULL) {
	  if (m->mms_resolvefuncs->mms_resolvermodule_init == NULL ||
	      m->mms_resolvefuncs->mms_resolve == NULL ||
	      m->mms_resolvefuncs->mms_resolvermodule_fini == NULL)
	       panic(0, "Missing or NULL functions in resolver module!");
     } else 
	  m->mms_resolvefuncs = &mms_resolvefuncs;	/* The default one. */

     m->mms_resolver_module_data = m->mms_resolvefuncs->mms_resolvermodule_init(octstr_get_cstr(m->resolver_params));

     m->detokenizer_params = _mms_cfg_getx(grp, octstr_imm("detokenizer-module-parameters"));
     
     /* Get and load the detokenizer lib if any. */
     if ((m->mms_detokenizefuncs = _mms_load_module(grp, "detokenizer-library", 
					       "mms_detokenizefuncs",
	       &mms_detokenizefuncs_shell)) != NULL) {
	  if (m->mms_detokenizefuncs->mms_detokenizer_init == NULL ||
	      m->mms_detokenizefuncs->mms_detokenize == NULL ||
	      m->mms_detokenizefuncs->mms_gettoken == NULL ||
	      m->mms_detokenizefuncs->mms_detokenizer_fini == NULL)
	       panic(0, "Missing or NULL functions in detokenizer module!");
	  if (m->mms_detokenizefuncs->mms_detokenizer_init(octstr_get_cstr(m->detokenizer_params)) != 0)
	      panic(0, "Detokenizer module failed to initialize");
     } else 
	  m->mms_detokenizefuncs = NULL;

     if (mms_cfg_get_bool(grp, octstr_imm("allow-ip-type"), &m->allow_ip_type)  < 0)
	  m->allow_ip_type = 1;
     
     mms_cfg_get_bool(grp, octstr_imm("optimize-notification-size"), 
		      &m->optimize_notification_size);

     if (mms_cfg_get_bool(grp, octstr_imm("content-adaptation"), &m->content_adaptation) < 0)
	  m->content_adaptation = 1;

     if (mms_cfg_get_bool(grp, octstr_imm("send-dlr-on-fetch"), &m->dlr_on_fetch) < 0)
	  m->dlr_on_fetch = 0;
     

     octstr_destroy(qdir);
     
     /* Now load the VASP list. */
     l = mms_cfg_get_multi(cfg, octstr_imm("mms-vasp"));
     m->vasp_list = gwlist_create();
     for (i=0, n=gwlist_len(l); i<n; i++) {
	  mCfgGrp *grp = gwlist_get(l, i);
	  MmsVasp *mv = gw_malloc(sizeof *mv);
	  Octstr *s;
	  int ibool = 0;
	  
	  mv->id = _mms_cfg_getx(grp, octstr_imm("vasp-id"));	  
	  mv->short_code = -1;
	  mms_cfg_get_int(grp, octstr_imm("short-code"), &mv->short_code);

	  mv->vasp_username = _mms_cfg_getx(grp, octstr_imm("vasp-username"));
	  mv->vasp_password = _mms_cfg_getx(grp, octstr_imm("vasp-password"));
	  
	  mv->vasp_url = _mms_cfg_getx(grp, octstr_imm("vasp-url"));
	  
	  s = _mms_cfg_getx(grp, octstr_imm("type"));

	  if (octstr_case_compare(s, octstr_imm("soap")) == 0)
	       mv->type = SOAP_VASP;
	  else if (octstr_case_compare(s, octstr_imm("eaif")) == 0)
	       mv->type = EAIF_VASP;
	  else
	       mv->type = NONE_VASP;
	  octstr_destroy(s);

	  mv->ver.major = mv->ver.minor1 = mv->ver.minor2 = 0;
	  if ((s = mms_cfg_get(grp, octstr_imm("mm7-version"))) != NULL && 
	      octstr_len(s) > 0) 
	       sscanf(octstr_get_cstr(s), "%d.%d.%d", &mv->ver.major, &mv->ver.minor1, &mv->ver.minor2);
	  else {
	       if (mv->type == SOAP_VASP) {
		    mv->ver.major =  MAJOR_VERSION(DEFAULT_MM7_VERSION);
		    mv->ver.minor1 = MINOR1_VERSION(DEFAULT_MM7_VERSION);
		    mv->ver.minor2 = MINOR2_VERSION(DEFAULT_MM7_VERSION);
	       } else if (mv->type == EAIF_VASP) {
		    mv->ver.major  = 3;
		    mv->ver.minor1 = 0;
	       }	       
	  }
	  octstr_destroy(s);
	  
	  if ((s = mms_cfg_get(grp, octstr_imm("mm7-soap-xmlns"))) != NULL) {
	       strncpy(mv->ver.xmlns, octstr_get_cstr(s), sizeof mv->ver.xmlns);	       

	       mv->ver.xmlns[-1 + sizeof mv->ver.xmlns] = 0; /* NULL terminate, just in case. */
	       octstr_destroy(s);
	  } else 
	       mv->ver.xmlns[0] = 0;

	  mv->ver.use_mm7_namespace = 1;
	  mms_cfg_get_bool(grp, octstr_imm("use-mm7-soap-namespace-prefix"), &mv->ver.use_mm7_namespace);

	  /* Set the handler vasp accounts. */
	  if (mms_cfg_get_bool(grp, octstr_imm("mms-to-email-handler"), &ibool) == 0 &&
	      ibool) {
	       if (m->mms2email)
		    warning(0, "mms-to-email handler VASP specified more than once! Only last config taken.");
	       m->mms2email = mv;
	  }	  
	  if (mms_cfg_get_bool(grp, octstr_imm("mms-to-local-copy-handler"), &ibool) == 0 &&
	      ibool) {
	       if (m->mms2mobile)
		    warning(0, "mms-to-mobile copy handler VASP specified more than once! Only last config taken."); 
	       m->mms2mobile = mv;
	  }
	  
	  if ((s = mms_cfg_get(grp, octstr_imm("send-uaprof"))) != NULL){ 	       
	       if (octstr_str_case_compare(s, "url") == 0)
		    mv->send_uaprof = UAProf_URL;
	       else if (octstr_str_case_compare(s, "ua") == 0)
		    mv->send_uaprof = UAProf_UA;
	       else {
		    warning(0, "unknown send-uaprof value '%s'. Must be \"ua\" or \"url\"!", 
			    octstr_get_cstr(s)); 
		    mv->send_uaprof = UAProf_None;
	       }
	       octstr_destroy(s);
	  }
	  gwlist_append(m->vasp_list, mv);
     }
     gwlist_destroy(l, NULL);
     return m;
}

List *mms_proxy_relays(mCfg *cfg, Octstr *myhostname)
{
     List *gl = mms_cfg_get_multi(cfg, octstr_imm("mmsproxy"));
     int i, n;
     List *l = gwlist_create();
     
     for (i = 0, n = gwlist_len(gl); i < n; i++) {
	  mCfgGrp *grp = gwlist_get(gl, i);
	  MmsProxyRelay *m = gw_malloc(sizeof *m);
	  Octstr *s;

	  m->host = _mms_cfg_getx(grp, octstr_imm("host"));
	  m->name = _mms_cfg_getx(grp, octstr_imm("name"));
	  m->allowed_prefix = _mms_cfg_getx(grp, octstr_imm("allowed-prefix"));
	  m->denied_prefix = _mms_cfg_getx(grp, octstr_imm("denied-prefix"));	  
	  if (mms_cfg_get_bool(grp, octstr_imm("confirmed-delivery"), &m->confirmed_mm4) < 0)
	       m->confirmed_mm4 = 1;
	  
	  m->sendmail = mms_cfg_get(grp, octstr_imm("send-mail-prog"));      	  
	  m->unified_prefix = mms_cfg_get(grp, octstr_imm("unified-prefix"));          
	  if ((s = mms_cfg_get(grp, octstr_imm("strip-prefixes"))) != NULL) {
	       m->strip_prefixes = octstr_split(s, octstr_imm(";"));
	       octstr_destroy(s);
	  } else 
	       m->strip_prefixes = NULL;

	  if (octstr_compare(m->host, myhostname) == 0)
	       warning(0, "MMSC Config: Found MM4 Proxy %s with same hostname as local host!", 
		       octstr_get_cstr(m->name));
	  gwlist_append(l, m);
     }
     
     gwlist_destroy(gl, NULL);

     return l;
}

Octstr *mms_makefetchurl(char *qf, Octstr *token, int loc,
			 Octstr *to,
			 MmscSettings *settings)
{
     Octstr *url = octstr_create("");
     Octstr *host_alias = settings->host_alias;
     Octstr *hstr; 
     Octstr *endtoken, *x; 

     MmsDetokenizerFuncStruct *tfs = settings->mms_detokenizefuncs;
     
     if (host_alias && octstr_len(host_alias) > 0)
	  hstr = octstr_duplicate(host_alias);
     else 
	  hstr = octstr_format("%S:%d",
			       settings->hostname, settings->port);
     
     octstr_format_append(url, "http://%S/%s@%d", 
			  hstr, 
			  qf, loc);
     
     if (tfs && tfs->mms_gettoken) { /* we append the recipient token or we append the message token. */
	  endtoken =  tfs->mms_gettoken(to); 
	  if (!endtoken) 
	       endtoken = octstr_imm("x");
     } else {
	  if (!token) 
	       endtoken = octstr_imm("x");	  
	  else
	       endtoken = octstr_duplicate(token);
     }
     
     x = octstr_duplicate(endtoken); /* might be immutable, so we duplicate it. */
     octstr_url_encode(x);
     octstr_format_append(url, "/%S", x);

     octstr_destroy(endtoken);
     octstr_destroy(x);
     octstr_destroy(hstr);
     return url;
}


Octstr *mms_find_sender_msisdn(Octstr *send_url, 
			       Octstr *ip,
			       List *request_hdrs, 
			       Octstr *msisdn_header, 
			       Octstr *requestip_header,
			       MmsDetokenizerFuncStruct* detokenizerfuncs)
{
     /* Either we have a WAP gateway header as defined, or we look for 
      * last part of url, pass it to detokenizer lib if defined, and back comes our number.
      */
     
     Octstr *phonenum = http_header_value(request_hdrs, 
					  msisdn_header);
     
     if (phonenum == NULL || octstr_len(phonenum) == 0) {
	  List *l  = octstr_split(send_url, octstr_imm("/"));
	  int len = gwlist_len(l);
	  Octstr *xip = http_header_value(request_hdrs, 
					  requestip_header);
	  if (xip == NULL)
	       xip = ip ? octstr_duplicate(ip) : NULL;
	  if (detokenizerfuncs && (len > 1 || xip)) 
	       phonenum = detokenizerfuncs->mms_detokenize(len > 1 ?  gwlist_get(l, len - 1) : send_url, 
							   xip);	  

	  gwlist_destroy(l, (gwlist_item_destructor_t *)octstr_destroy);
	  octstr_destroy(xip);
     }
     
     return phonenum;     
}

Octstr *mms_find_sender_ip(List *request_hdrs, Octstr *ip_header, Octstr *ip, int *isv6)
{
     Octstr *xip;
     /* Look in the headers, if none is defined, return actual IP */
     Octstr *client_ip = http_header_value(request_hdrs, ip_header);
     char *s;
     
     xip = client_ip  ? client_ip : octstr_duplicate(ip);
     
     s = octstr_get_cstr(xip);

     /* Crude test for ipv6 */
     *isv6 = (index(s, ':') != NULL);
     return xip;
}

int mms_decodefetchurl(Octstr *fetch_url,  
		       Octstr **qf, Octstr **token, int *loc)
{
     Octstr *xfurl = octstr_duplicate(fetch_url);
     int i, j, n;
     char *s, *p;
     
     for (i = 0, n = 0, s = octstr_get_cstr(xfurl); 
	  i < octstr_len(xfurl); i++)
	  if (s[i] == '/')
	       n++;
     if (n < 2) /* We need at least two slashes. */
	  octstr_append_char(xfurl, '/');
     
     i = 0;
     n = octstr_len(xfurl);
     s = octstr_get_cstr(xfurl);
     
     p = strrchr(s, '/'); /* Find last slash. */
     if (p)
	  i = (p - s) - 1;
     else
	  i = n-1;
     
     if (i < 0)
	  i = 0;
     
     while (i>0 && s[i] != '/')
	  i--; /* Go back, find first slash */
     if (i>=0 && s[i] == '/')
	  i++;
     
     /* Now we have qf, find its end. */
     
     j = i;     
     while (j<n && s[j] != '/')
	  j++; /* Skip to next slash. */
     
     *qf = octstr_copy(fetch_url, i, j-i);
     
     if (j<n)
	  *token = octstr_copy(fetch_url, j + 1, n - (j+1));
     else
	  *token = octstr_create("");
     octstr_destroy(xfurl);

     /* Now get loc out of qf. */
     *loc = MMS_LOC_MQUEUE;
     i = octstr_search_char(*qf, '@', 0);
     if (i >= 0) {
	  long l;
	  int j = octstr_parse_long(&l, *qf, i+1, 10);
	  if (j > 0)
	       *loc = l;
	  octstr_delete(*qf, i, octstr_len(*qf));
     } 
     
     return 0;
}


int mms_ind_send(Octstr *prov_cmd, Octstr *to)
{
     Octstr *tmp;
     Octstr *s;
     int res = 1;
     
     if (prov_cmd == NULL ||
	 octstr_len(prov_cmd) == 0)
	  return 1;
     
     tmp = octstr_duplicate(to);
     escape_shell_chars(tmp);     
     s = octstr_format("%S %S", prov_cmd, tmp); 
     octstr_destroy(tmp);

     if (s) {
	  int x = system(octstr_get_cstr(s));
	  int y = WEXITSTATUS(x);

	  if (x < 0) {
	       error(0, "Checking MMS Ind.Send: Failed to run command %s!", 
		     octstr_get_cstr(s));
	       res = 1;	
	  } else if (y != 0 && y != 1)
	       res =  -1;
	  else 
	       res = y;
	  octstr_destroy(s);
     } else 
	  warning(0, "Checking MMS Ind.Send: Failed call to compose command [%s] ", 
	       octstr_get_cstr(prov_cmd));

	  
     return res;
}

void notify_prov_server(char *cmd, char *from, char *event, char *arg, Octstr *msgid, 
			Octstr *ua, Octstr *uaprof)
{
     Octstr *s;
     Octstr *tmp, *tmp2, *tmp3, *tmp4;
     
     if (cmd == NULL || cmd[0] == '\0')
	  return;

     gw_assert(from);
     gw_assert(cmd);
     gw_assert(event);

     tmp = octstr_create(from);     
     tmp2 = msgid ? octstr_duplicate(msgid) : octstr_create("");     
     tmp3 = ua ? octstr_duplicate(ua) : octstr_create("");
     tmp4 = uaprof ? octstr_duplicate(uaprof) : octstr_create("");

     escape_shell_chars(tmp);     
     escape_shell_chars(tmp2);
     escape_shell_chars(tmp3);
     escape_shell_chars(tmp4);
     
     s = octstr_format("%s '%s' '%S' '%s' '%S' '%S' '%S'", cmd, event, 
		       tmp, arg, tmp2, tmp3, tmp4); 

     system(octstr_get_cstr(s));	  
     
     octstr_destroy(s);
     octstr_destroy(tmp);
     octstr_destroy(tmp2);
     octstr_destroy(tmp3);
     octstr_destroy(tmp4);     
}
