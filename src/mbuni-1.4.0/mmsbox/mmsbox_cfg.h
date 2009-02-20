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
#ifndef __MMSBOX_CFG_INCLUDED__
#define __MMSBOX_CFG_INCLUDED__
#include "mms_util.h"
#include "mmsbox_mt_filter.h"
#include "mms_queue.h"
#include "mmsbox_resolve.h"

#include "mmsbox_mmsc.h"

typedef struct MxmscGrp {
     Octstr *id;       /* MMSC id (for logging). */
     Octstr *group_id; /* GROUP MMSC id (used for qf). */
	 Octstr *vasp_id;
     Octstr *mmsc_url; /* URL at which MMSC can be reached. */
     struct {
	  Octstr *user, *pass;
	  Octstr *allow_ip;
	  Octstr *deny_ip;     	  
	  long port;
     } incoming;      /* user, pass, port (and whether SSL) that MMSC uses to connect to us. */
	 int eaif_http_accept_response;
	
     Octstr *allowed_prefix,  *denied_prefix;
     Octstr *allowed_sender_prefix,  *denied_sender_prefix;
 	 List *subst_patterns;
     enum {UNKNOWN_MMSC = -1, CUSTOM_MMSC, SOAP_MMSC, MPES_MMSC, EAIF_MMSC} type; /* type of connection. */
     double  throughput;  /* Max send rate.  */
     long threadid;   /* handler thread. */
  
     int reroute;     /* whether messages from this mmsc are re-routed outwards. */
     Octstr *reroute_mmsc_id;
     int no_senderaddress; /* used by SOAP interface: Don't add a sender address. */
     int reroute_mod_subject; /* Set to true if we'll change subject line on reroute. */
     MM7Version_t ver; /* supported MM7/SOAP version. */
     int use_mt_filter; /* whether to use MT filter on this connection. */
     Mutex *mutex;
     
     MmsBoxMmscFuncs *fns; /* pointer to functions for handling this mmsc connection type */
     Octstr *settings;     /* settings for the above module. */
     void *data;           /* data for above module. */
     int custom_started;   /* set to 1 if custom mmc started. */

     List *incoming_substitution_patterns;
     List *outgoing_substitution_patterns;

     List *match_part_order;

     Dict *seen_messages; /* A collection of message id's of message previously handled by this connection.
							 This is used to avoid receiving duplicate messages. */

     int asynchronous;
	 long msgid_expire;
} MmscGrp;

typedef struct MmsServiceUrlParam {
     Octstr *name;
     enum {NO_PART, AUDIO_PART, IMAGE_PART, VIDEO_PART, 
	   TEXT_PART, SMIL_PART , OTHER_PART, 
	   ANY_PART, WHOLE_BINARY, KEYWORD_PART} type;
     Octstr *value; /* for generic value (type == NO_PART), 
		     * or for value that follows spec (e.g. %Tisatest is allowed) 
		     */
} MmsServiceUrlParam;

typedef struct MmsService {
     Octstr *name;         /* name of service. */
     int isdefault;
     int omitempty;
     int noreply;
     int accept_x_headers;
     List *passthro_headers;
     
     int assume_plain_text;
     List   *keywords;  /* List of keywords matched. */
     enum {TRANS_TYPE_GET_URL, TRANS_TYPE_POST_URL, TRANS_TYPE_FILE, TRANS_TYPE_EXEC, 
	   TRANS_TYPE_TEXT} type;
     Octstr *url;        /* The value. */
     List   *params;     /* of MmsServiceUrlParam */
     
     Octstr *faked_sender;
     List   *allowed_mmscs; /* List of MMSCs allowed to access this service (by ID). */
     List   *denied_mmscs;  /* List of MMSCs allowed to access this service (by ID). */
     Octstr *service_code;  /* Service code (MM7/SOAP only) */

     Octstr *allowed_receiver_prefix,  *denied_receiver_prefix;
	 Octstr *override_subject; // If set, the subject will be changed to the value of this

     regex_t *allowed_sender_regex, *allowed_receiver_regex;
	 
     regex_t *regex;
} MmsService;

typedef struct SendMmsUser {
     Octstr *user, *pass;
     Octstr *faked_sender;
     Octstr *dlr_url, *rr_url;
} SendMmsUser;

/* Basic settings for the mmsbox. */
extern List *sendmms_users; /* list of SendMmsUser structs */
extern List *mms_services;  /* list of MMS Services */
extern List *mmscs;         /* MMSC list. Perhaps turn into a Dict instead? */
extern List *sub_patterns;
extern Octstr *incoming_qdir, *outgoing_qdir, *dlr_dir, *response_qdir;
extern Octstr *unified_prefix;
extern List *strip_prefixes;
extern long mmsbox_maxsendattempts, mmsbox_send_back_off, default_msgexpiry;
extern long  maxthreads;
extern double queue_interval;
extern long eaif_http_accept_response;

extern struct SendMmsPortInfo {
     long port; /* Might be ssl-ed. */
     Octstr *allow_ip;
     Octstr *deny_ip;
} sendmms_port;

extern struct AdminPortInfo {
	long port;
	Octstr *allow_ip;
	Octstr *deny_ip;
} admin_port;

typedef struct SubPattern {
	Octstr *id;
	regex_t *match_pattern;
	Octstr *replacement;
} SubPattern;

extern struct MmsBoxMTfilter *mt_filter;
extern  MmsQueueHandlerFuncs *qfs;
extern int mt_multipart;

extern MmsBoxResolverFuncStruct *rfs; /* resolver functions. */
extern void *rfs_data;
extern Octstr *rfs_settings;


extern int mms_load_mmsbox_settings(mCfg *cfg, gwthread_func_t *mmsc_handler_func);
extern MmscGrp *get_handler_mmc(Octstr *id, Octstr *to, Octstr *from);

extern Octstr  *get_mmsbox_queue_dir(Octstr *from, List *to, MmscGrp *m, 
				     Octstr **mmc_id);
extern void mmsbox_cleanup_mmsc_settings(void);

extern Octstr* admin_password;

#endif
