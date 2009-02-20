/*
 * Mbuni - Open  Source MMS Gateway 
 * 
 * MMSBox CFG: MMBox configuration and misc. functions
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
#include "mmsbox_cfg.h"
#include "mms_queue.h"
#include "mmsbox_resolve_shell.h"

List *sendmms_users = NULL; /* list of SendMmsUser structs */
List *mms_services = NULL;  /* list of MMS Services */
List *mmscs = NULL;
List *substitution_patterns = NULL; /* substitution patterns for number handling */
Octstr *incoming_qdir, *outgoing_qdir, *dlr_dir, *response_qdir;
long mmsbox_maxsendattempts, mmsbox_send_back_off, default_msgexpiry;
long  maxthreads = 0;
double queue_interval = -1;
Octstr *unified_prefix = NULL;
List *strip_prefixes = NULL;
long eaif_http_accept_response = HTTP_NO_CONTENT;


int mt_multipart = 0;
MmsQueueHandlerFuncs *qfs; /* queue functions. */
MmsBoxResolverFuncStruct *rfs; /* resolver functions. */
void *rfs_data;
Octstr *rfs_settings; 

Octstr *admin_user, *admin_password;

struct SendMmsPortInfo sendmms_port;
struct AdminPortInfo admin_port;

struct MmsBoxMTfilter *mt_filter = NULL;

int mms_load_mmsbox_settings(mCfg *cfg, gwthread_func_t *mmsc_handler_func)
{     
     mCfgGrp *grp = mms_cfg_get_single(cfg, octstr_imm("mbuni"));
     mCfgGrp *cgrp = mms_cfg_get_single(cfg, octstr_imm("core"));
     Octstr *gdir = NULL, *s, *tmp;
	 int send_port_ssl = 0, admin_port_ssl = 0;
	 long max_memory_queued;
     List *l;
     int i, n, xx;
     void *catchall = NULL;

     if (grp == NULL)
	  panic(0,"Missing required group `mbuni' in config file!");

     mms_load_core_settings(cgrp);
     
     sendmms_users = gwlist_create();
     mms_services = gwlist_create();
     mmscs = gwlist_create();
     substitution_patterns = gwlist_create();

     if (mms_cfg_get_int(grp, 
			 octstr_imm("maximum-send-attempts"), &mmsbox_maxsendattempts) < 0)
	  mmsbox_maxsendattempts = MAXQTRIES;
     
     if (mms_cfg_get_int(grp, 
		     octstr_imm("send-attempt-back-off"), &mmsbox_send_back_off) == -1)
	  mmsbox_send_back_off = BACKOFF_FACTOR;

     if (mms_cfg_get_int(grp, octstr_imm("default-message-expiry"), &default_msgexpiry) == -1)
	  default_msgexpiry = DEFAULT_EXPIRE;

     if (mms_cfg_get_int(grp, octstr_imm("max-send-threads"), &maxthreads) == -1)
          maxthreads = 10;
     
     s = mms_cfg_get(grp, octstr_imm("queue-run-interval"));
     if (s) {
	  queue_interval = atof(octstr_get_cstr(s));
	  octstr_destroy(s);
     }
     if (queue_interval <= 0)
	  queue_interval = QUEUERUN_INTERVAL;

     gdir = mms_cfg_get(grp, octstr_imm("storage-directory"));
	 
	 mms_cfg_get_int(grp, octstr_imm("max-memory-queued"), &max_memory_queued);

     if (gdir == NULL)
	  gdir = octstr_imm(".");
     
     if (mkdir(octstr_get_cstr(gdir), S_IRWXU|S_IRWXG) < 0 && errno != EEXIST)
	  panic(0, "Failed to create MMSBox storage directory: %s - %s!",
		octstr_get_cstr(gdir), strerror(errno));
     
     if ((qfs = _mms_load_module(grp, "queue-manager-module", "qfuncs", NULL)) == NULL) {
	  qfs = &default_qfuncs; /* default queue handler. */
	  qfs->mms_init_queue_module(gdir, maxthreads, max_memory_queued);
     } else {
	  Octstr *s = _mms_cfg_getx(grp, octstr_imm("queue-module-init-data"));
	  if (qfs->mms_init_queue_module(s, maxthreads,max_memory_queued) != 0)
	       panic(0, "failed to initialise queue module, with data: %s",
		     octstr_get_cstr(s));
	  octstr_destroy(s);
     }

     if ((incoming_qdir = qfs->mms_init_queue_dir("mmsbox_incoming", &xx)) == NULL ||
	 xx != 0)
	  panic(0, "Failed to initialise incoming mmsbox queue directory: %s - %s!", 
		octstr_get_cstr(incoming_qdir), strerror(errno));

     if ((outgoing_qdir = qfs->mms_init_queue_dir("mmsbox_outgoing", &xx)) == NULL ||
	 xx != 0)
	  panic(0, "Failed to initialise outgoing mmsbox queue directory: %s - %s!", 
		octstr_get_cstr(outgoing_qdir), strerror(errno));
		
     if ((response_qdir = qfs->mms_init_queue_dir("mmsbox_responses", &xx)) == NULL ||
	 xx != 0)
	  panic(0, "Failed to initialise response mmsbox queue directory: %s - %s!", 
		octstr_get_cstr(response_qdir), strerror(errno));

     /* XXX still uses old-style file storage. */
     if (qfs != &default_qfuncs) 
       default_qfuncs.mms_init_queue_module(gdir, maxthreads, max_memory_queued);
     if ((dlr_dir = default_qfuncs.mms_init_queue_dir("mmsbox_dlr", &xx)) == NULL ||
	 xx != 0) 
	  panic(0, "Failed to initialise dlr storage directory: %s - %s!", 
		octstr_get_cstr(dlr_dir), strerror(errno));

          
     unified_prefix = _mms_cfg_getx(grp, octstr_imm("unified-prefix"));  
     
     if ((s = mms_cfg_get(grp, octstr_imm("strip-prefixes"))) != NULL) {
	  strip_prefixes = octstr_split(s, octstr_imm(";"));
	  octstr_destroy(s);
     } else 
	  strip_prefixes = NULL;

     mms_cfg_get_int(grp, octstr_imm("sendmms-port"), &sendmms_port.port);
#ifdef HAVE_LIBSSL
     mms_cfg_get_bool(grp, octstr_imm("sendmms-port-ssl"), &send_port_ssl);
#endif
     if (http_open_port(sendmms_port.port, send_port_ssl) < 0)
	  error(0, "MMSBox: Failed to start sendmms HTTP server on %ld: %s!",
		sendmms_port.port,
		strerror(errno));
     
     sendmms_port.allow_ip = mms_cfg_get(grp, octstr_imm("allow-ip"));          
     sendmms_port.deny_ip =  mms_cfg_get(grp, octstr_imm("deny-ip"));

     mms_cfg_get_int(grp, octstr_imm("admin-port"), &admin_port.port);
#ifdef HAVE_LIBSSL
     mms_cfg_get_bool(grp, octstr_imm("sendmms-port-ssl"), &admin_port_ssl);
#endif
	 if (admin_port.port > 0) {
     	if (http_open_port(admin_port.port, admin_port_ssl) < 0)
	  		error(0, "MMSBox: Failed to start admin HTTP server on %ld: %s!",
			admin_port.port,
			strerror(errno));
		/* Admin interface configuration */
		admin_password = mms_cfg_get(grp,octstr_imm("admin-password"));
    	admin_port.allow_ip = mms_cfg_get(grp, octstr_imm("admin-allow-ip"));
    	admin_port.deny_ip = mms_cfg_get(grp, octstr_imm("admin-deny-ip"));
	}

     /* load the filter if any. */
     if ((mt_filter = _mms_load_module(grp, "mmsbox-mt-filter-library", "mmsbox_mt_filter", NULL)) != NULL)
     info(0, "MMSBox: Loaded MT Filter [%s]", mt_filter->name);

     mms_cfg_get_bool(grp, octstr_imm("mmsbox-mt-always-multipart"), &mt_multipart);

     /* load the resolver module. */
     if ((rfs = _mms_load_module(grp, "resolver-library", "mmsbox_resolvefuncs", &mmsbox_resolvefuncs_shell)) == NULL)
	  rfs = &mmsbox_resolvefuncs;

     rfs_settings = _mms_cfg_getx(grp, octstr_imm("resolver-module-parameters"));
     rfs_data = rfs->mmsbox_resolvermodule_init(rfs_settings ? octstr_get_cstr(rfs_settings) : NULL);

	mms_cfg_get_int(grp, octstr_imm("eaif-http-accept-response"), &eaif_http_accept_response);
			
     /* Now get sendmms users. */     
     l = mms_cfg_get_multi(cfg, octstr_imm("send-mms-user"));
     for (i = 0, n = gwlist_len(l); i < n; i++) {
	  mCfgGrp *x = gwlist_get(l, i);
	  SendMmsUser *u = gw_malloc(sizeof *u);

	  memset(u, 0, sizeof *u);
	  
	  u->user = _mms_cfg_getx(x, octstr_imm("username"));
	  u->pass = _mms_cfg_getx(x, octstr_imm("password"));
	  u->faked_sender = mms_cfg_get(x, octstr_imm("faked-sender"));	  	  
	  u->dlr_url = _mms_cfg_getx(x, octstr_imm("delivery-report-url"));	  	  
	  u->rr_url = _mms_cfg_getx(x, octstr_imm("read-report-url"));	  	  
	  gwlist_append(sendmms_users, u);
     }    
     gwlist_destroy(l, NULL);

     /* Get substitution pattern list. */
     l = mms_cfg_get_multi(cfg, octstr_imm("substitution-pattern"));
     for (i = 0, n = gwlist_len(l); i < n; i++) {
		mCfgGrp *x = gwlist_get(l,i);
		SubPattern *s = gw_malloc(sizeof *s);
		Octstr *re;
		
        s->id = _mms_cfg_getx(x, octstr_imm("id"));
        if (octstr_len(s->id) < 1)
          panic(0,"Missing required value `id' in config file!");
        re = _mms_cfg_getx(x,octstr_imm("match-pattern"));
        if (octstr_len(re) < 1)
          panic(0,"Missing required value `match-pattern' in config file!");
        s->match_pattern = gw_regex_comp(re, REG_EXTENDED);
        if (s->match_pattern == NULL)
          panic(0, "Regex compilation for match-pattern failed");

        s->replacement = _mms_cfg_getx(x,octstr_imm("replacement"));
        if (octstr_len(s->replacement) < 1)
          panic(0,"Missing required value `replacement' in config file!");
        
        gwlist_append(substitution_patterns, s);
     }

     /* Get mmsc list. */
     l = mms_cfg_get_multi(cfg, octstr_imm("mmsc"));
     for (i = 0, n = gwlist_len(l); i < n; i++) {
	  mCfgGrp *x = gwlist_get(l, i);
	  MmscGrp *m = gw_malloc(sizeof *m);
	  int ssl = 0;
	  Octstr *type;
	  Octstr *xver;
	  Octstr *s;

	  memset(m, 0, sizeof *m);
	  
	  m->id = _mms_cfg_getx(x, octstr_imm("id"));
          if (octstr_len(m->id) < 1)
	       panic(0,"Missing required value `id' in config file!");
          m->group_id = mms_cfg_get(x, octstr_imm("group-id"));
          if (m->group_id == NULL)
	       m->group_id = octstr_duplicate(m->id);
	
	  m->vasp_id = mms_cfg_get(x, octstr_imm("vasp-id"));
	  if (m->vasp_id == NULL)
		m->vasp_id = octstr_duplicate(m->id);

	  m->mmsc_url = _mms_cfg_getx(x, octstr_imm("mmsc-url"));

	  m->allowed_prefix = mms_cfg_get(x, octstr_imm("allowed-prefix"));
	  m->denied_prefix = mms_cfg_get(x, octstr_imm("denied-prefix"));

	  m->allowed_sender_prefix = mms_cfg_get(x, octstr_imm("allowed-sender-prefix"));
	  m->denied_sender_prefix = mms_cfg_get(x, octstr_imm("denied-sender-prefix"));

	  m->incoming.allow_ip = mms_cfg_get(x, octstr_imm("allow-ip"));
	  m->incoming.deny_ip =  mms_cfg_get(x, octstr_imm("deny-ip"));

          info(0, "MMSC[%s], allow=[%s], deny=[%s] group_id=[%s]", 
	       octstr_get_cstr(m->id),
	       octstr_get_cstr(m->incoming.allow_ip), 
	       octstr_get_cstr(m->incoming.deny_ip), 
	       octstr_get_cstr(m->group_id));

	  m->incoming.user = _mms_cfg_getx(x, octstr_imm("incoming-username"));
	  m->incoming.pass = _mms_cfg_getx(x, octstr_imm("incoming-password"));
	  mms_cfg_get_int(x, octstr_imm("incoming-port"), &m->incoming.port);
#ifdef HAVE_LIBSSL
	  mms_cfg_get_bool(x, octstr_imm("incoming-port-ssl"), &ssl);
#endif
          if ((tmp = mms_cfg_get(x, octstr_imm("max-throughput"))) != NULL) {   
               if (octstr_parse_double(&m->throughput, tmp, 0) == -1)
		    m->throughput = 0;
               info(0, "Set throughput to %.3f for mmsc id <%s>", 
		    m->throughput, octstr_get_cstr(m->id));
               octstr_destroy(tmp);
          }

		m->incoming_substitution_patterns = gwlist_create();
		m->outgoing_substitution_patterns = gwlist_create();
		int what;
		for (what = 0; what < 2; what++) {
			List* target_list = NULL;
			Octstr *cfg_key = NULL;
			switch(what) {
				case 0: 
					target_list = m->incoming_substitution_patterns;
					cfg_key = octstr_imm("incoming-number-substitutions");
					break;
				case 1:
					target_list = m->outgoing_substitution_patterns;
					cfg_key = octstr_imm("outgoing-number-substitutions");
					break;
			}
			
			if ((tmp = mms_cfg_get(x,cfg_key))) {
				List *mmsc_patterns = octstr_split(tmp, octstr_imm(","));
				debug("", 0, "split(%s) pattern length=%ld", octstr_get_cstr(tmp), gwlist_len(mmsc_patterns));
				Octstr *current_pattern;

				int i;
				for(i = 0; i < gwlist_len(mmsc_patterns); i++) {
					current_pattern = gwlist_get(mmsc_patterns,i);
					octstr_dump(current_pattern,0);
					debug("", 0, "pattern=%s",octstr_get_cstr(current_pattern));
					octstr_strip_blanks(current_pattern);
				
					// Check if the pattern exists
					int j, match = 0;
					for (j = 0; j < gwlist_len(substitution_patterns); j++) {
						SubPattern *sp;
						sp = gwlist_get(substitution_patterns, j);
						debug("",0,"sp=[%s]", octstr_get_cstr(sp->id));
						if (octstr_compare(sp->id, current_pattern) == 0) {
							match = 1;
							break;
						}
					}
					if (match == 0)
						panic(0, "Couldn't find match pattern in mmsc configuration");
						
					gwlist_append(target_list, current_pattern);
				}
				gwlist_destroy(mmsc_patterns, NULL);
			}
		}
		

		m->match_part_order = gwlist_create();
		if ((tmp = mms_cfg_get(x,octstr_imm("match-part-order")))) {
				m->match_part_order = octstr_split(tmp, octstr_imm(","));
				
				// Make sure we trim spaces
				for (i = 0; i < gwlist_len(m->match_part_order); i++)
					octstr_strip_blanks(gwlist_get(m->match_part_order, i));
					
		} else { // default case - match only in first found text attachment
			gwlist_append(m->match_part_order, octstr_create("text"));
			gwlist_append(m->match_part_order, octstr_create("subject"));
		}
		

	  type = _mms_cfg_getx(x, octstr_imm("type"));
	  if (octstr_case_compare(type, octstr_imm("eaif")) == 0)
	       m->type = EAIF_MMSC;
	  else if (octstr_case_compare(type, octstr_imm("soap")) == 0)
	       m->type = SOAP_MMSC;
	  else if (octstr_case_compare(type, octstr_imm("mpes")) == 0)
			m->type = MPES_MMSC;
	  else if (octstr_case_compare(type, octstr_imm("custom")) == 0) {
	       m->type = CUSTOM_MMSC;
	       m->settings = _mms_cfg_getx(x, octstr_imm("custom-settings"));	       
	       /* also load the libary. */
	       if ((m->fns = _mms_load_module(x, "mmsc-library", "mmsc_funcs", NULL)) == NULL)
		    panic(0, "failed to load MMSC libary functions from module!");
	  } else 
	       warning(0, "MMSBox: Unknown MMSC type [%s]!", 
		       octstr_get_cstr(type));
	  if ((xver = _mms_cfg_getx(x, octstr_imm("mm7-version"))) != NULL && 
	      octstr_len(xver) > 0)
	       sscanf(octstr_get_cstr(xver), 
		      "%d.%d.%d", 
		      &m->ver.major, &m->ver.minor1, &m->ver.minor2);
	  else { /* Put in some defaults. */
	       if (m->type == SOAP_MMSC) { 
		    m->ver.major =  MAJOR_VERSION(DEFAULT_MM7_VERSION);
		    m->ver.minor1 = MINOR1_VERSION(DEFAULT_MM7_VERSION);
		    m->ver.minor2 = MINOR2_VERSION(DEFAULT_MM7_VERSION);
	       } else if (m->type == EAIF_MMSC) {
		    m->ver.major  = 3;
		    m->ver.minor1 = 0;
	       }
	  }
	
		if (mms_cfg_get_bool(x, octstr_imm("asynchronous-eaif-mode"), &m->asynchronous) == -1)
			m->asynchronous = 0;
			
		if (mms_cfg_get_bool(x, octstr_imm("detect-duplicate-messages"), &xx) == -1)
			m->seen_messages = NULL;
		else 
			m->seen_messages = xx ? dict_create(100, octstr_destroy_item) : NULL;
			
			
		mms_cfg_get_int(x, octstr_imm("msgid-expire"), &m->msgid_expire);
		if (m->msgid_expire == -1)
			m->msgid_expire = 24 * 60 * 60; // Default expire is 24

	  if ((s = mms_cfg_get(x, octstr_imm("mm7-soap-xmlns"))) != NULL) {
	       strncpy(m->ver.xmlns, octstr_get_cstr(s), sizeof m->ver.xmlns);
	       m->ver.xmlns[-1 + sizeof m->ver.xmlns] = 0; /* NULL terminate, just in case. */
	       octstr_destroy(s);
	  } else 
	       m->ver.xmlns[0] = 0;

	  m->ver.use_mm7_namespace = 1;
	  mms_cfg_get_bool(x, octstr_imm("use-mm7-soap-namespace-prefix"), &m->ver.use_mm7_namespace);

	  octstr_destroy(xver);
	  octstr_destroy(type);

	  /* Init for filter. */
	  if ((s = mms_cfg_get(x, octstr_imm("mm7-mt-filter-params"))) != NULL) {
	       if (mt_filter)
		    m->use_mt_filter = (mt_filter->init(m->mmsc_url, m->id, s) == 1);
	       else 
		    panic(0, "MMSBox: mt-filter-params set for MMSC[%s] but no MT-filter lib "
			  "specified!", 
			  octstr_get_cstr(m->id));
	       if (!m->use_mt_filter)
		    warning(0, "MMSBox: MT MMS filter turned off for MMSC[%s]. Init failed",
			    octstr_get_cstr(m->id));			    
	       octstr_destroy(s);
	  } else 
	       m->use_mt_filter = 0;

	  mms_cfg_get_bool(x, octstr_imm("reroute"), &m->reroute);
	  mms_cfg_get_bool(x, octstr_imm("reroute-add-sender-to-subject"), &m->reroute_mod_subject);
	  m->reroute_mmsc_id = mms_cfg_get(x, octstr_imm("reroute-mmsc-id"));
	  if (m->reroute_mmsc_id != NULL && m->reroute == 0) 
	       warning(0, "MMSBox: reroute-mmsc-id parameter set but reroute=false!");
	  
	  mms_cfg_get_bool(x, octstr_imm("no-sender-address"), &m->no_senderaddress);
	  m->mutex = mutex_create();

	  /* finally start the thingie. */
	  if (m->type == CUSTOM_MMSC) {
	       if (m->fns->start_conn(m, qfs, unified_prefix, strip_prefixes, &m->data) != 0) {
		    warning(0, "MMSBox: Failed to start custom MMSC [%s]", octstr_get_cstr(m->id));
		    m->custom_started = 0;
	       } else 
		    m->custom_started = 1;
	  } else {
	       if (m->incoming.port > 0 && 
		   http_open_port(m->incoming.port, ssl) < 0) {
		    warning(0, "MMSBox: Failed to start HTTP server on receive port for "
			    " MMSC %s, port %ld: %s!",
			    octstr_get_cstr(m->id), m->incoming.port,
			    strerror(errno));
		    m->incoming.port = 0; /* so we don't listen on it. */
	       }
	       
	       if (mmsc_handler_func && m->incoming.port > 0) { /* Only start threads if func passed and ... */
		    if ((m->threadid = gwthread_create(mmsc_handler_func, m)) < 0)
			 error(0, "MMSBox: Failed to start MMSC handler thread for MMSC[%s]: %s!",
			       octstr_get_cstr(m->id), strerror(errno));
	       } else 
		    m->threadid = -1;
	  }

	  gwlist_append(mmscs, m);	  
     }     
     gwlist_destroy(l, NULL);

     l = mms_cfg_get_multi(cfg, octstr_imm("mms-service"));     
     for (i = 0, n = gwlist_len(l); i < n; i++) {
	  mCfgGrp *x = gwlist_get(l, i);
	  MmsService *m = gw_malloc(sizeof *m);
	  Octstr *s;
	  
	  m->name = _mms_cfg_getx(x, octstr_imm("name"));
	  if ((m->url = mms_cfg_get(x, octstr_imm("get-url"))) != NULL)
	       m->type = TRANS_TYPE_GET_URL;
	  else if ((m->url = mms_cfg_get(x, octstr_imm("post-url"))) != NULL)
	       m->type = TRANS_TYPE_POST_URL;
	  else if ((m->url = mms_cfg_get(x, octstr_imm("file"))) != NULL)
	       m->type = TRANS_TYPE_FILE;
	  else if ((m->url = mms_cfg_get(x, octstr_imm("exec"))) != NULL)
	       m->type = TRANS_TYPE_EXEC;
	  else if ((m->url = mms_cfg_get(x, octstr_imm("text"))) != NULL)
	       m->type = TRANS_TYPE_TEXT;
	  else 
	       panic(0, "MMSBox: Service [%s] has no url!", octstr_get_cstr(m->name));
	  
	  m->faked_sender = mms_cfg_get(x, octstr_imm("faked-sender"));	  	  
	  
	  m->isdefault = 0;
	  mms_cfg_get_bool(x, octstr_imm("catch-all"), &m->isdefault);     
	  if (m->isdefault) {
	       if (catchall)
		    warning(0, "MMSBox: Multiple default mms services defined!");	       
	       catchall = m;
	  }

	  if (mms_cfg_get_bool(x, octstr_imm("omit-empty"), &m->omitempty) < 0)
	       m->omitempty = 0;
	  if (mms_cfg_get_bool(x, octstr_imm("suppress-reply"), &m->noreply) < 0)
	       m->noreply = 0;
	  mms_cfg_get_bool(x, octstr_imm("accept-x-mbuni-headers"), &m->accept_x_headers); 

	  if ((s = mms_cfg_get(x, octstr_imm("pass-thro-headers"))) != NULL) {
	       m->passthro_headers = octstr_split(s, octstr_imm(","));
	       octstr_destroy(s);
	  } else 
	       m->passthro_headers = NULL;

	  mms_cfg_get_bool(x, octstr_imm("assume-plain-text"), &m->assume_plain_text);

	  if ((s = mms_cfg_get(x, octstr_imm("accepted-mmscs"))) != NULL) {
	       m->allowed_mmscs = octstr_split(s, octstr_imm(";"));
	       octstr_destroy(s);
	  } else 
	       m->allowed_mmscs = NULL; /* means allow all. */

	  if ((s = mms_cfg_get(x, octstr_imm("denied-mmscs"))) != NULL) {
	       m->denied_mmscs = octstr_split(s, octstr_imm(";"));
	       octstr_destroy(s);
	  } else 
	       m->denied_mmscs = NULL; /* means allow all. */

	  m->allowed_receiver_prefix = mms_cfg_get(x, octstr_imm("allowed-receiver-prefix"));
	  m->denied_receiver_prefix = mms_cfg_get(x, octstr_imm("denied-receiver-prefix"));
	 
	  /* Get key words. Start with aliases to make life easier. */
	  if ((s = mms_cfg_get(x, octstr_imm("aliases"))) != NULL) {
	       m->keywords = octstr_split(s, octstr_imm(";"));
	       octstr_destroy(s);
	  } else 
	       m->keywords = gwlist_create();
	  
	  s = mms_cfg_get(x, octstr_imm("keyword"));
	  if (s) 
	       gwlist_append(m->keywords, s);
	
      s = mms_cfg_get(x, octstr_imm("regex"));
      if (!s)
		m->regex = NULL;
	  else {
		if (NULL == (m->regex = gw_regex_comp(s, REG_EXTENDED)))
         	panic(0, "Regex compilation for regular expression %s failed", octstr_get_cstr(s));
	  }
	
	  if (m->regex == NULL && gwlist_len(m->keywords) == 0)
		panic(0, "MMSBox: Service [%s] has no keyword or regular expression defined!", octstr_get_cstr(m->name));


	s = mms_cfg_get(x, octstr_imm("allowed-receiver-regex"));
	if (!s)
		m->allowed_receiver_regex = NULL;
	else {
		if (NULL == (m->allowed_receiver_regex = gw_regex_comp(s, REG_EXTENDED)))
         		panic(0, "Regex compilation for regular expression %s failed", octstr_get_cstr(s));
	}


	s = mms_cfg_get(x, octstr_imm("allowed-sender-regex"));
	if (!s)
		m->allowed_sender_regex = NULL;
	else {
		if (NULL == (m->allowed_sender_regex = gw_regex_comp(s, REG_EXTENDED)))
         		panic(0, "Regex compilation for regular expression %s failed", octstr_get_cstr(s));
	}
	
	  m->override_subject = mms_cfg_get(x,octstr_imm("override-subject"));
	  
	  if ((s = mms_cfg_get(x, octstr_imm("http-post-parameters"))) != NULL) {
	       List *r = octstr_split(s, octstr_imm("&"));
	       int i, n;
	       m->params = gwlist_create();
	       if (m->type != TRANS_TYPE_POST_URL)
		    warning(0, "MMSBox:  Service [%s] specifies HTTP Post parameters "
			    "without specifying post-url type/url!", octstr_get_cstr(m->name));
	       for (i = 0, n = gwlist_len(r); i < n; i++) {
		    Octstr *y = gwlist_get(r, i);
		    int ii = octstr_search_char(y, '=', 0);
		    if (ii < 0)
			 ii = octstr_len(y);
		    
		    if (ii > 0) {
			 MmsServiceUrlParam *p = gw_malloc(sizeof *p);			 
			 int ch;
			 p->name = octstr_copy(y, 0, ii);
			 p->value = NULL;
			 p->type = NO_PART;

			 if (octstr_get_char(y, ii+1) == '%') {
			      switch(ch = octstr_get_char(y, ii+2)) {
			      case 'a':
				   p->type = AUDIO_PART; break;
			      case 'b':
				   p->type = WHOLE_BINARY; break;
			      case 'i':
				   p->type = IMAGE_PART; break;
			      case 'v':
				   p->type = VIDEO_PART; break;				   
			      case 't':
				   p->type = TEXT_PART; break;				   
			      case 's':
				   p->type = SMIL_PART; break;				   
			      case 'o':
				   p->type = OTHER_PART; break;				   
			      case 'z':
				   p->type = ANY_PART; break;				   
			      case 'k':
				p->type = KEYWORD_PART; break;
			      case '%':
				   p->type = NO_PART; break;
			      default:
				   warning(0, "MMSBox: Unknown conversion character %c "
					   "in http-post-parameters. Service [%s]!",
					   ch, octstr_get_cstr(m->name));
				   p->type = NO_PART;
				   break;
			      }
			      p->value = octstr_copy(y, ii+3, octstr_len(y));
			 } else { /* No conversion spec. */
			      p->type = NO_PART; 
			      p->value = octstr_copy(y, ii+1, octstr_len(y));
			 }
			 gwlist_append(m->params, p);
		    } else 
			 warning(0, "MMSBox: Missing http-post-parameter name? Service [%s]!",
				 octstr_get_cstr(m->name));
	       }
	       gwlist_destroy(r, (gwlist_item_destructor_t *)octstr_destroy);
	       octstr_destroy(s);
	  } else 
	       m->params = NULL;

	  m->service_code = mms_cfg_get(x, octstr_imm("service-code"));
	

	  gwlist_append(mms_services, m);
     }
     gwlist_destroy(l, NULL);
     octstr_destroy(gdir);

     return 0;
}

/* Get the MMC that should handler this recipient. */
MmscGrp *get_handler_mmc(Octstr *id, Octstr *to, Octstr *from)
{
     MmscGrp *mmc = NULL, *res = NULL;
     int i,  n;
     Octstr *phonenum = NULL, *xfrom = NULL;

     if (id) /* If ID is set, use it. */
	  for (i = 0, n = gwlist_len(mmscs); i  < n; i++)
	       if ((mmc = gwlist_get(mmscs, i)) != NULL &&
		   mmc->id && octstr_compare(mmc->id, id) == 0) 
		    return mmc;

     if (octstr_search_char(to, '@', 0) > 0 || 
	 octstr_case_search(to, octstr_imm("/TYPE=IPv"), 0) > 0) /* For emails, or ip take first mmsc. */
	  return gwlist_get(mmscs, 0); 
     
     /* now try allow/deny stuff. */
     phonenum = extract_phonenum(to, unified_prefix);
     xfrom = extract_phonenum(from, NULL);     
     
     for (i = 0, n = gwlist_len(mmscs); i  < n; i++)  {
	  if ((mmc = gwlist_get(mmscs, i)) == NULL)
	       continue;
	  
	  if (mmc->allowed_prefix && 
	      does_prefix_match(mmc->allowed_prefix, phonenum) == 0)
	       continue; /* does not match. */
	  
	  if (mmc->denied_prefix && 
	      does_prefix_match(mmc->denied_prefix, phonenum) != 0)
	       continue;  /* matches. */

	  if (mmc->allowed_sender_prefix && 
	      does_prefix_match(mmc->allowed_sender_prefix, xfrom) == 0)
	       continue; /* does not match. */
	  
	  if (mmc->denied_sender_prefix && 
	      does_prefix_match(mmc->denied_sender_prefix, xfrom) != 0)
	       continue;  /* matches. */
	  
	  res = mmc; /* otherwise it matches, so go away. */
	  break;
     }
     
     octstr_destroy(phonenum);
     octstr_destroy(xfrom);
     
     return res;
}

/* handle message routing. */
Octstr  *get_mmsbox_queue_dir(Octstr *from, List *to, MmscGrp *m, 
		     Octstr **mmc_id) 
{
    
     if (m->reroute) {
	  *mmc_id = m->reroute_mmsc_id ? octstr_duplicate(m->reroute_mmsc_id) : NULL;
	  return outgoing_qdir;
     } else {
	  Octstr *_mcid, *qdir = NULL;
	  Octstr *fto;
	  
	  if (gwlist_len(to) > 0 && 
	      (fto = gwlist_extract_first(to)) != NULL) { /* we route based on first recipient XXX */
	       Octstr *xto = octstr_duplicate(fto);
	       Octstr *xfrom = octstr_duplicate(from);
	

	       if (unified_prefix) 
		    _mms_fixup_address(&xfrom, octstr_get_cstr(unified_prefix), strip_prefixes, 0);
	       if (unified_prefix)
		    _mms_fixup_address(&fto, octstr_get_cstr(unified_prefix), strip_prefixes, 0);

	       _mcid = rfs->mmsbox_resolve(xfrom,fto,octstr_get_cstr(m->id), rfs_data, rfs_settings);
	       
	       /* modify what was sent to us. */
	       if (octstr_len(_mcid) == 0) { /* put recipient back, unmodified if incoming only. */	       
		    gwlist_insert(to, 0, xto); 
		    octstr_destroy(fto);
	       } else {
		    if (unified_prefix)
			 _mms_fixup_address(&fto, octstr_get_cstr(unified_prefix), strip_prefixes, 1);		    
		    gwlist_insert(to, 0, fto);
		    octstr_destroy(xto);
	       }
		    
	       if (xfrom) {
		    octstr_delete(from, 0, octstr_len(from));
		    octstr_append(from, xfrom);
	       }
	       octstr_destroy(xfrom);
	  } else 
	       _mcid = NULL;
	  
	  if (octstr_len(_mcid) == 0) {
	       *mmc_id = NULL;
	       qdir = incoming_qdir;
	  } else {
	       *mmc_id = octstr_duplicate(_mcid);
	       qdir = outgoing_qdir;
	  }

	  octstr_destroy(_mcid);

	  return qdir;
     }
     return 0;
}

void mmsbox_cleanup_mmsc_settings(void)
{
     /* eventually we will destroy the object. For now, we only cleanup queue module. */
     if (qfs) 
	  qfs->mms_cleanup_queue_module();
}
