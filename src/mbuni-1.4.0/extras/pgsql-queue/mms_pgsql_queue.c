#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <libpq-fe.h>
#include "mms_queue.h"

/*
 * Mbuni - Open  Source MMS Gateway 
 * 
 * Mbuni Queue handler module using PostgreSQL database storage
 * 
 * Copyright (C) 2007-2008, Digital Solutions Ltd. - http://www.dsmagic.com
 *
 * Paul Bagyenda <bagyenda@dsmagic.com>
 * 
 * This program is free software, distributed under the terms of
 * the GNU General Public License, with a few exceptions granted (see LICENSE)
 */


/* first we need the db connection pooling. */
#define DEFAULT_CONNECTIONS 5
#define MIN_QRUN_INTERVAL 2 /* we don't want to hurt DB. */
#define MQF 'q'

static List *free_conns;
static int pool_size;
static int pgq_init_module(Octstr *conninfo, int max_threads)
{
     long i, n = 0;
     
     gw_assert(conninfo);
     i = octstr_search_char(conninfo,':', 0);
     if (i>0) {
	  n = strtoul(octstr_get_cstr(conninfo), NULL, 10);
	  octstr_delete(conninfo, 0, i+1);
     } 
     
     if (n <= 0)
	  n = DEFAULT_CONNECTIONS;
     else if (n <= max_threads * 4) { /* must all each thread up to 2 connections, and then some.*/

	  n = max_threads*4 + 1;

	  info(0, "pgsql_queue_init: Forced number of DB connections to %d", (int)n);	       
     }
     free_conns = gwlist_create();
     gwlist_add_producer(free_conns);     
     for (i = 0; i<n;i++) {
	  PGconn *c = PQconnectdb(octstr_get_cstr(conninfo));
	  if (c && PQstatus(c) == CONNECTION_OK) {
	       gwlist_produce(free_conns, c);	
	       pool_size++;
	  } else  {
	       error(0, "pgsql_queue.init: failed to connect to db: %s", 
		     PQerrorMessage(c));	    
	       PQfinish(c);
	  }	
     }
     
     srand(time(NULL));
     return gwlist_len(free_conns) > 0 ? 0 : -1;
}

static int pgq_cleanup_module(void)
{
     gw_assert(free_conns);

     gwlist_remove_producer(free_conns);
     sleep(2);
     gwlist_destroy(free_conns, (void *)PQfinish);
     free_conns = NULL;
     pool_size = 0;
     return 0;
}

#define get_conn() get_conn_real(__FUNCTION__, __FILE__, __LINE__)
#define return_conn(conn) return_conn_real((conn), __FUNCTION__, __FILE__, __LINE__)
#define DEFAULT_PG_WAIT 30 /* fail after 2 minutes. */
static PGconn *get_conn_real(const char *function, const char *file, const int line)
{
     PGconn *c;
     PGresult *r;
 
     if (free_conns == NULL) return NULL;
     
     c = gwlist_timed_consume(free_conns, DEFAULT_PG_WAIT);     

     /* debug("pg_cp",0, "pg_get_conn> %s:%d, %s => %d", file, line, function, (int)c); */

     if (c) { /* might fail if we are shutting down. */
	  r = PQexec(c, "BEGIN"); /* start a transaction. */
	  PQclear(r);
     } else 
	  error(0, "pg_get_conn: Failed to get a free connection from connection pool! Consider increasing pool size (currently %d)?",
		pool_size);
     return c;
}

static void return_conn_real(PGconn *c, const char *function, const char *file, const int line)
{
     PGresult *r;
     
     /*  debug("pg_cp", 0, "pg_release_conn> %s:%d, %s => %d", file, line, function, (int)c); */

     if (free_conns == NULL) return;
     
     /* commit or destroy transaction. */
     if (PQtransactionStatus(c) == PQTRANS_INERROR)
	  r = PQexec(c, "ROLLBACK");
     else 
	  r = PQexec(c, "COMMIT");
     PQclear(r);
     gwlist_produce(free_conns,c);
}


/* structure for use within the Envelope thingie. */
struct pgfile_t {
     PGconn *conn;
     char dir[256+1];
     char _pad[4]; /* paranoia */
     int64_t qid; /* internal key into table (if any) */          
};


static Octstr *pgq_init_queue_dir(char *qdir, int *error)
{
     gw_assert(free_conns);
     if (error) *error = 0;
     return octstr_create(qdir); /* just make a string out of it. nothing more to do. */
}


static int pgq_free_envelope(MmsEnvelope *e, int removefromqueue)
{
     int ret = 0;

     struct pgfile_t *qfs;
     if (e == NULL)
	  return 0;
     
     qfs = e->qfs_data;
     gw_assert(qfs->conn);
     
     if (removefromqueue) {
	  char cmd[256];
	  /* copy to separate table. */
	  PGresult *res;
	  sprintf(cmd, "INSERT INTO archived_mms_messages SELECT "
		  " * from mms_messages WHERE id = %lld", qfs->qid);
	  res = PQexec(qfs->conn, cmd);
	  
	  if (PQresultStatus(res) != PGRES_COMMAND_OK) {	       
	       PQclear(res);	       
	       ret = -1;
	       goto done;
	  } else 
	       PQclear(res);
	  
	  sprintf(cmd, "INSERT INTO archived_mms_message_headers SELECT "
		  " * from mms_message_headers WHERE qid = %lld", qfs->qid);
	  res = PQexec(qfs->conn, cmd);

	  if (PQresultStatus(res) != PGRES_COMMAND_OK) {	       
	       PQclear(res);
	       ret = -1;
	       goto done;
	  } else 
	       PQclear(res);
	
	  sprintf(cmd, "DELETE from mms_messages WHERE id = %lld", qfs->qid);
	  res = PQexec(qfs->conn, cmd);
	  if (PQresultStatus(res) != PGRES_COMMAND_OK) 
	       ret = -1;
	  PQclear(res);
     }
     
 done:
     return_conn(qfs->conn);
     mms_queue_free_envelope(e);
     return ret;
}

/* Queue header 'item' names -- copied largely from file-based scheme.
 * - each header represented by single letter, followed by 
 *   a parameter. Letters mean:
 * T - message type (full text string -- MMS message type.
 * I - message ID
 * F - From address 
 * R - Recipient (the ones pending) for this message
 * Z - Recipient (ones already routed to) for this message.
 * P - Proxy who sent it to us
 * p - Proxy through which this message shd be delivered (e.g. delivery report)
 * S - Message size
 * s - Message subject.
 * f - time of last content fetch 
 * t - user defined token.
 * b - billed amount.
 * r - whether delivery receipts are required or not.
 * M - Application specific data (string)
 * V - VASPID -- from VASP
 * v - vasid -- from VASP
 * U - url1 -- e.g. for delivery report
 * u - url2 -- e.g. for read report
 * H - generic headers associated with message (e.g. for passing to MMC)
 */

static MmsEnvelope *pgq_queue_readenvelope(char *qf, char *mms_queuedir, int shouldblock)
{
     int64_t qid;
     long num_attempts, i, n;
     time_t sendt, created, lastt, edate;
     char cmd[4*QFNAMEMAX], _qf[QFNAMEMAX*2+1];
     Octstr *from = NULL;
     
     MmsEnvelope *e = NULL;
     struct pgfile_t *pgs;
     
     PGconn *c = get_conn(); /* grab yourself a connection. */
     PGresult *r = NULL;
     char *s;
               
     if (c == NULL)
	  return NULL;
     n = strlen(qf);
     PQescapeStringConn(c, _qf, qf, n < QFNAMEMAX ? n : QFNAMEMAX, NULL);

     /* read and block, to ensure no one else touches it. */
     sprintf(cmd, "SELECT id,cdate,lastt,sendt,edate,num_attempts,sender FROM "
	     " mms_messages_view WHERE qdir='%s' AND qfname = '%s' FOR UPDATE %s",
	     mms_queuedir, _qf, 
	     shouldblock ? "" : "NOWAIT"); /* nice little PostgreSQL 8.x addition. */
     r = PQexec(c, cmd);
     
     if (PQresultStatus(r) != PGRES_TUPLES_OK || PQntuples(r) < 1) {
	  return_conn(c);
	  goto done;
     }

     /* Get top-level values. */
     if ((s = PQgetvalue(r, 0, PQfnumber(r, "id"))) != NULL)
	  qid = strtoull(s, NULL, 10);
     else 
	  qid = 0;


     if ((s = PQgetvalue(r, 0, PQfnumber(r, "cdate"))) != NULL)
	  created = strtoul(s, NULL, 10);
     else 
	  created = 0;

     if ((s = PQgetvalue(r, 0, PQfnumber(r, "lastt"))) != NULL)
	  lastt = strtoul(s, NULL, 10);
     else 
	  lastt = 0;

     if ((s = PQgetvalue(r, 0, PQfnumber(r, "sendt"))) != NULL)
	  sendt = strtoul(s, NULL, 10);
     else 
	  sendt = 0;

     if ((s = PQgetvalue(r, 0, PQfnumber(r, "edate"))) != NULL)
	  edate = strtoul(s, NULL, 10);
     else 
	  edate = 0;

     if ((s = PQgetvalue(r, 0, PQfnumber(r, "num_attempts"))) != NULL)
	  num_attempts = strtoul(s, NULL, 10);
     else 
	  num_attempts = 0;

     if ((s = PQgetvalue(r, 0, PQfnumber(r, "sender"))) != NULL)
	  from = octstr_create(s);

     PQclear(r);

     sprintf(cmd, "SELECT item,value,id FROM mms_message_headers WHERE qid=%lld FOR UPDATE", qid);     
     r = PQexec(c, cmd);     

     if (PQresultStatus(r) != PGRES_TUPLES_OK) {
	  return_conn(c);
	  goto done;
     }
     
     /* now create the structure. */
     e = mms_queue_create_envelope(from, NULL, 
				   NULL, 
				   NULL, NULL, 
				   0, 0, 
				   NULL, 
				   NULL, NULL, 
				   NULL, NULL, 
				   NULL,
				   0, 
				   NULL, 
				   NULL,
				   qf,
				   NULL,
				   sizeof (struct pgfile_t), NULL);
     
     /* set the private data. */
     pgs = e->qfs_data;
     pgs->conn = c;
     pgs->qid = qid;
     strncpy(pgs->dir, mms_queuedir, sizeof pgs->dir);
     
     /* set some top-level stuff. */
     e->created = created;
     e->lasttry = lastt;
     e->sendt = sendt;
     e->expiryt = edate;
     e->attempts = num_attempts;

     if (mms_validate_address(e->from) != 0) {
	  warning(0, "mms_queueread: Mal-formed address [%s] in queue entry %s/%s! "
		  "Attempting fixup.", octstr_get_cstr(e->from), mms_queuedir, qf);
	  _mms_fixup_address(&e->from, NULL,NULL,1);
     }
     
     /* now read the headers... */
     for (i = 0, n = PQntuples(r); i<n;i++) {	 
	  char *item = PQgetvalue(r, i, 0);
	  char *res = PQgetvalue(r, i, 1);
	  char *ptmp = PQgetvalue(r, i, 2); 
	  long hid = ptmp ? atoi(ptmp) : 0;

	  if (!item || !res) continue;
	  
	  switch (item[0]) {	       
	       Octstr *t;
	       MmsEnvelopeTo *to;
	  case 'T':
	       t = octstr_create(res);
	       e->msgtype = mms_string_to_message_type(t);
	       octstr_destroy(t);
	       if (e->msgtype < 0) {
		    e->msgtype = 0;
		    error(0, "mms_queueread: Unknown MMS message type (%s) in queue entry %s/%s, skipped!\n",
			  res, mms_queuedir, qf);
	       }
	       break;
	  case 'I':
	       e->msgId = octstr_create(res);	       
	       break;
	  case 'i': /* interface. */
	       strncpy(e->src_interface, res, sizeof e->src_interface);	       
	    break;
	  case 'R':
	  case 'Z':
	       t = octstr_create(res);
	       if (mms_validate_address(t) != 0) {
		    warning(0, "mms_queueread: Mal-formed address [%s] in queue entry %s/%s! " 
			    "Attempting fixup.", res, mms_queuedir, qf);
		    _mms_fixup_address(&t, NULL,NULL,1);
	       }
	       to = gw_malloc(sizeof *to);
	       to->rcpt = t;
	       to->process = (item[0] == 'Z') ? 0 : 1;	       
	       gwlist_append(e->to, to);
	       break;
	  case 'P':
	       e->fromproxy = octstr_create(res);
	       break;
	  case 'M':
	       e->mdata = octstr_create(res);
	       break;
	  case 'p':
	       e->viaproxy = octstr_create(res);
	       break;
	  case 'S':
	       e->msize = atol(res);
	    break;
	  case 's':
	       e->subject = octstr_create(res);
	       break;	
	  case 't':
	       e->token = octstr_create(res);
	       break;
	  case 'f':
	       e->lastaccess = atol(res);
	       break;
	  case 'b':
	       e->bill.billed = 1;
	       e->bill.amt = atof(res);
	    break;
	  case 'r':
	       e->dlr = 1;
	       break;
	  case 'V':
	       e->vaspid = octstr_create(res);
	       break;
	  case 'v':
	       e->vasid = octstr_create(res);
	       break;

	  case 'U':
	       e->url1 = octstr_create(res);
	       break;

	  case 'u':
	       e->url2 = octstr_create(res);
	       break;
	  case 'H':
	       if (e->hdrs == NULL)
		    e->hdrs = http_create_empty_headers();
	       if ((ptmp = index(res, ':')) == NULL)
		    error(0, "Incorrectly formatted line header [id=%ld] in queue file %s/%s!",
			  hid, mms_queuedir, qf);
	       else {
		    char *value = ptmp + 1;
		    char hname[512];
		    int xlen = (ptmp - res < sizeof hname) ? ptmp - res : -1 + sizeof hname;
		    strncpy(hname, res, xlen);
		    hname[xlen] = 0; /* terminate it. */
		    http_header_add(e->hdrs, hname, value);
	       }
	       break;
	  default:
	       error(0, "Unknown QF header %c in file %s/%s, skipped!", item[0], mms_queuedir, qf);
	       break;
	  }
     }
     
 done:
     if (r)
	  PQclear(r);
     return e;
}

/* utility writer function. */
static int _puthdr(PGconn *c, int64_t qid, char *hname, char *val)
{
     char cmd[QFNAMEMAX*4+1], vbuf[2*QFNAMEMAX+1];
     PGresult *r;
     int ret, n;
     
     n = strlen(val);
     PQescapeStringConn(c, vbuf, val, n < QFNAMEMAX ? n : QFNAMEMAX, NULL);
     
     sprintf(cmd, "INSERT INTO mms_message_headers (qid,item,value) VALUES (%lld, '%.128s', '%.128s')",
	     qid, hname, vbuf);

     r = PQexec(c, cmd);
     ret = (PQresultStatus(r) == PGRES_COMMAND_OK) ? 0 : -1;
     PQclear(r);
     
     return ret;
}


static int writeenvelope(MmsEnvelope *e, int newenv)
{
     char *s, buf[512], cmd[QFNAMEMAX*4 + 1], lastt[128], sendt[128], expiryt[128], *xfrom;
     int i, n;
     struct pgfile_t *qfs = e ? e->qfs_data : NULL;
     PGresult *r;
     
     gw_assert(e);
     
     if (!newenv) {
	  sprintf(cmd, "DELETE FROM mms_message_headers WHERE qid = %lld", qfs->qid);
	  r = PQexec(qfs->conn, cmd);
	  PQclear(r);
     }

     /* Write out. */

     /* first the top-level stuff... */
     

     if (e->lasttry) 
	  sprintf(lastt, ", last_try='epoch'::timestamp with time zone+ '%ld secs'::interval",  
		  e->lasttry);
     else 
	  lastt[0] = 0;
     
     if (e->sendt) 
	  sprintf(sendt, ", send_time='epoch'::timestamp with time zone + '%ld secs'::interval",  
		  e->sendt);
     else 
	  sendt[0] = 0;
     
     if (e->expiryt) 
	  sprintf(expiryt, ", expire_date='epoch'::timestamp with time zone + '%ld secs'::interval",  
		  e->expiryt);
     else
	  expiryt[0] = 0;
     
     xfrom = gw_malloc(2*octstr_len(e->from)+1);
     PQescapeStringConn(qfs->conn, xfrom, octstr_get_cstr(e->from), octstr_len(e->from), NULL);     
     sprintf(cmd, "UPDATE mms_messages SET num_attempts = %ld, sender='%s' %s %s %s WHERE id = %lld",
	     e->attempts, xfrom, lastt, sendt, expiryt, qfs->qid);
     gw_free(xfrom);

     r = PQexec(qfs->conn, cmd);
     
     if (PQresultStatus(r) != PGRES_COMMAND_OK)        
       error(0, "pgwriteenvelope: Failed to update queue entry %s in %s: %s",
	     e->xqfname, qfs->dir, PQresultErrorMessage(r));
     
     PQclear(r);
     
     /* then the rest... */
     s = (char *)mms_message_type_to_cstr(e->msgtype);
     if (!s) {
	  error(0, "mms_queuewrite: Unknown MMS message type %d! Skipped\n", e->msgtype);
	  s = "";
     }
     _puthdr(qfs->conn, qfs->qid, "T", s);
     
     if (e->msgId) 
	  _puthdr(qfs->conn, qfs->qid, "I", octstr_get_cstr(e->msgId));

     if (e->to)
	  n = gwlist_len(e->to);
     else
	  n = 0;

     for (i = 0; i < n; i++) {
	  MmsEnvelopeTo *to = gwlist_get(e->to, i);
	 	 
	  _puthdr(qfs->conn, qfs->qid, 
		  (to->process)	? "R" : "Z", 
		  octstr_get_cstr(to->rcpt));
     }

     /* Output headers if any. */
     n = (e->hdrs) ? gwlist_len(e->hdrs) : 0;
     for (i = 0; i < n; i++) {
	  Octstr *h = NULL, *v = NULL;

	  http_header_get(e->hdrs, i, &h, &v);
	  if (h && v) {
	       Octstr *x = octstr_format("%s:%s", octstr_get_cstr(h), 
					 octstr_get_cstr(v));
	       _puthdr(qfs->conn, qfs->qid, "H", octstr_get_cstr(x));
	       octstr_destroy(x);	       
	  }
	  if (h) octstr_destroy(h);
	  if (v) octstr_destroy(v);

     }

     if (e->lastaccess) {
	  sprintf(buf, "%ld", e->lastaccess);
	  _puthdr(qfs->conn, qfs->qid, "f", buf);
     }

     sprintf(buf, "%ld", e->msize);
     _puthdr(qfs->conn, qfs->qid, "S", buf);


     if (e->fromproxy) 
	  _puthdr(qfs->conn, qfs->qid, "P", octstr_get_cstr(e->fromproxy));



     if (e->src_interface[0]) 
	  _puthdr(qfs->conn, qfs->qid, "i", e->src_interface);

     if (e->mdata) 
	  _puthdr(qfs->conn, qfs->qid, "M", octstr_get_cstr(e->mdata));

     if (e->subject)
	  _puthdr(qfs->conn, qfs->qid, "s", octstr_get_cstr(e->subject));
     

     if (e->viaproxy) 
	  _puthdr(qfs->conn, qfs->qid, "p", octstr_get_cstr(e->viaproxy));

     if (e->token) 
	  _puthdr(qfs->conn, qfs->qid, "t", octstr_get_cstr(e->token));
     

      if (e->vaspid) 
	  _puthdr(qfs->conn, qfs->qid, "V", octstr_get_cstr(e->vaspid));
     
      if (e->vasid) 
	  _puthdr(qfs->conn, qfs->qid, "v", octstr_get_cstr(e->vasid));
     
      if (e->url1) 
	  _puthdr(qfs->conn, qfs->qid, "U", octstr_get_cstr(e->url1));

      if (e->url2) 
	  _puthdr(qfs->conn, qfs->qid, "u", octstr_get_cstr(e->url2));

     if (e->dlr) 
	  _puthdr(qfs->conn, qfs->qid, "r", "Yes");

     if (e->bill.billed) {
	  sprintf(buf, "%.3f", e->bill.amt);
	  _puthdr(qfs->conn, qfs->qid,"b", buf);
     }
     
     return 0;
}


static Octstr *pgq_queue_add(Octstr *from, List *to, 
			     Octstr *subject,
			     Octstr *fromproxy, Octstr *viaproxy, 
			     time_t senddate, time_t expirydate, MmsMsg *m, Octstr *token, 
			     Octstr *vaspid, Octstr *vasid,
			     Octstr *url1, Octstr *url2,
			     List *hdrs,
			     int dlr,
			     char *directory, 
			     char *src_interface, 
			     Octstr *mmscname)
{
     char qf[QFNAMEMAX], xqid[128];
     int64_t qid;
     MmsEnvelope *e;
     Octstr *ms = NULL, *res = NULL, *xcmd = NULL;
     struct pgfile_t *qfs = NULL;
     PGconn *conn = get_conn();
     PGresult *r;
     char *data, *xfrom, *s;
     size_t dlen;
     static int ect;

     if (conn == NULL)
	  return NULL;

     /* get an ID for it. */
     r = PQexec(conn, "SELECT nextval('mms_messages_id_seq') as qid");
     if (PQresultStatus(r) != PGRES_TUPLES_OK || PQntuples(r) < 1) {
	  PQclear(r);
	  return_conn(conn);
	  return NULL;
     }
     s = PQgetvalue(r, 0, 0);
     gw_assert(s);

     qid = strtoull(s, NULL, 10);     
     PQclear(r);

     /* make the long queue id. Including the integer qid ensures uniqueness.*/
     sprintf(qf, "%cf%lld-%ld.%d.x%d.%ld", 
	     MQF, 
	     qid,
	     (long)time(NULL) % 10000, 
	     (++ect % 10000), getpid()%1000, random() % 100);

     res = octstr_create(qf);
    
     e = mms_queue_create_envelope(from, to, subject, fromproxy,viaproxy,
				   senddate,expirydate,token,vaspid,vasid,
				   url1,url2,hdrs,dlr,mmscname,m,
				   qf,
				   src_interface,
				   sizeof(struct pgfile_t), &ms);

     qfs = e->qfs_data;
     qfs->conn = conn;
     strncpy(qfs->dir, directory, sizeof qfs->dir);
     qfs->qid = qid;

     /* write the basic data: qid, qfname, qdir, sender, data, escape those that are not trusted. */

     xfrom = gw_malloc(2*octstr_len(from)+1);
     PQescapeStringConn(qfs->conn, xfrom, octstr_get_cstr(from), octstr_len(from), NULL);     
     data = (void *)PQescapeByteaConn(qfs->conn, (void *)octstr_get_cstr(ms), octstr_len(ms), &dlen);          
     
     sprintf(xqid, "%lld", qid);     
     xcmd = octstr_format("INSERT INTO mms_messages (id, qdir, qfname, sender, data,expire_date) VALUES "
			  " (%s, '%s', '%s', '%s', E'%s'::bytea, current_timestamp)",
			  xqid, directory, qf, xfrom, data);    
     PQfreemem(data);
     gw_free(xfrom);
     
     r = PQexec(qfs->conn, octstr_get_cstr(xcmd));

     octstr_destroy(xcmd);

     if (PQresultStatus(r) != PGRES_COMMAND_OK) {

       error(0, "mms_queue_add: Failed to add data for queue entry %s in %s: %s",
		e->xqfname, qfs->dir, PQresultErrorMessage(r));

	  PQclear(r);
	  octstr_destroy(res);
	  res = NULL;
	  goto done;
     } else
	  PQclear(r); 
  
     /* inserted it, now write fuller envelope. */
        
     if (writeenvelope(e, 1) < 0) {
	  octstr_destroy(res);
	  res = NULL;
	  goto done;
     }

 done:     
     pgq_free_envelope(e, 0); /* free thingie, relinquish connection. If error occured, this will cause a rollback.*/
     octstr_destroy(ms);

     return res;
}

static int pgq_queue_free_env(MmsEnvelope *e)
{
     return pgq_free_envelope(e,0);
}

/* taken exactly from file-based. XXX perhaps we didn't modularize right! */
static int pgq_queue_update(MmsEnvelope *e)
{
     int i, n = (e && e->to) ? gwlist_len(e->to) : 0;
     int hasrcpt = 0;
     MmsEnvelopeTo *x;     

     if (!e) return -1;
     /* FIX: Don't allow expiry to be <= 0 */
     if (e->expiryt <= 0)
	  e->expiryt = time(NULL) + DEFAULT_EXPIRE;
     for (i = 0; i < n; i++)	  
	  if ((x = gwlist_get(e->to, i)) != NULL && 
	      x->process) {
	       hasrcpt = 1;
	       break;
	  }
     
     if (!hasrcpt) {
	  pgq_free_envelope(e,1);
	  return 1;
     } else 
	  return writeenvelope(e, 0);         
}


static int pgq_queue_replacedata(MmsEnvelope *e, MmsMsg *m)
{
     char *data, xqid[128];
     size_t dlen;
     struct pgfile_t *qfs;
     int ret = 0;
     Octstr *ms, *xcmd;
     PGresult *r;
     
     if (!e) return -1;

     qfs = e->qfs_data;
     ms = mms_tobinary(m);
     
     data = (void *)PQescapeByteaConn(qfs->conn, (void *)octstr_get_cstr(ms), octstr_len(ms), &dlen);     
     sprintf(xqid, "%lld", qfs->qid);
     xcmd = octstr_format("UPDATE mms_messages SET data=E'%s' WHERE id = %s", 
			  data, xqid);
     r = PQexec(qfs->conn, octstr_get_cstr(xcmd));
     ret =  (PQresultStatus(r) != PGRES_COMMAND_OK) ? -1 : 0; /* do nothing about error. we are in a transaction.*/
     PQclear(r);
     octstr_destroy(ms);
     PQfreemem(data);
     octstr_destroy(xcmd);
     
     return ret;
}

static MmsMsg *pgq_queue_getdata(MmsEnvelope *e)
{
     struct pgfile_t *qfs;
     MmsMsg *m;
     Octstr *ms;
     size_t dlen, n;
     char cmd[512], *data, *x;
     PGresult *r;
     
     if (e == NULL)
	  return NULL;

     qfs = e->qfs_data;

     sprintf(cmd, "SELECT data from mms_messages WHERE id = %lld", qfs->qid);
     r = PQexec(qfs->conn, cmd);

     if (PQresultStatus(r) != PGRES_TUPLES_OK ||
	 (n = PQntuples(r)) < 1) {
	  PQclear(r);
	  error(0, "mms_queue_getdata: Failed to load data for queue entry %s in %s",
		e->xqfname, qfs->dir);
	  return NULL;
     }

     x = PQgetvalue(r, 0, 0);
     if (x && (isprint(x[0]) || x[0] == '\\')) {/* data was sent to us escapaed, so un-escape it. */
	  data = (void *)PQunescapeBytea((void *)x, &dlen);
     } else {
	  dlen = PQgetlength(r, 0, 0); /* get data length before you fetch it. */
	  data = x;
     }
     ms = octstr_create_from_data(data, dlen);
     if (x != data) PQfreemem(data);

     PQclear(r);
     if (ms == NULL) {
	  error(0, "mms_queue_getdata: Failed to read data for queue entry %s in %s",
		e->xqfname, qfs->dir);	  
	  return NULL;
     }

     m = mms_frombinary(ms, octstr_imm(""));
     if (!m) 
	  error(0, "mms_queue_getdata: Failed to decode data for queue entry %s in %s",
		e->xqfname, qfs->dir);

     octstr_destroy(ms);
     return m;
}

struct Qthread_data_t {
     int64_t qid;
     char qf[QFNAMEMAX]; 
     char dir[QFNAMEMAX]; /* item to load. */
 
     int (*deliver)(MmsEnvelope *e);    
};

static void pgdeliver(List *item_list)
{
     struct Qthread_data_t *d;
     
     while ((d = gwlist_consume(item_list)) != NULL) {
	  MmsEnvelope *e = pgq_queue_readenvelope(d->qf, d->dir, 0);
	  int res;
	  
	  if (e) { /* no need to check time -- it was checked in queue runner, so we know it's time to send.*/
	       debug("pgqueue_run", 0, "Queued entry %s/%s to thread %ld",
		     d->dir, d->qf, gwthread_self());
	       res = d->deliver(e);
	       
	       if (res != 1)
		    pgq_free_envelope(e, 0);
	  } 

	  gw_free(d);
     }
     /* we're done, exit. */
}

static void pgq_queue_run(char *dir, 
			  int (*deliver)(MmsEnvelope *), 
			  double sleepsecs, int num_threads, int *rstop)
{
     
     List *items_list = gwlist_create();
     int i, n;
     char cmd[512];
     long *th_ids = NULL;
     
     gw_assert(num_threads > 0);

     info(0, "pgsql_queue: Queue runner on [%s] startup...", dir);
     if (sleepsecs < MIN_QRUN_INTERVAL) {	  
	  warning(0, "minimum queue run interval for PG Queue module is %d secs.", MIN_QRUN_INTERVAL);
	  sleepsecs = MIN_QRUN_INTERVAL; 
     }
     
     gwlist_add_producer(items_list);
     th_ids = gw_malloc(num_threads*sizeof th_ids[0]);
     
     for (i = 0; i<num_threads; i++)
	  th_ids[i] = gwthread_create((gwthread_func_t *)pgdeliver, items_list);
     
     /* Note that we get messages ready for delivery (whether or not they have expired).
      * any other conditions will be handled by upper level,
      * e.g. expiry, recipients, and so on. 
      */
     sprintf(cmd, "SELECT id, qfname FROM mms_messages WHERE qdir = '%.128s' AND "
	     "send_time <= current_timestamp", dir);     
     do {
	  PGconn *c = get_conn();
	  PGresult *r;
	  char *qfname, *qid;
	  
	  if (c == NULL)
	       break;
	  r = PQexec(c, cmd);
	  if (PQresultStatus(r) == PGRES_TUPLES_OK && (n = PQntuples(r))> 0)
	       for (i = 0; i<n; i++) 
		    if ((qfname = PQgetvalue(r, i, 1)) !=  NULL && 
			(qid = PQgetvalue(r, i, 0)) != NULL) {
			 struct Qthread_data_t *d = gw_malloc(sizeof *d);
			 
			 memset(d, 0, sizeof *d);			 
			 d->qid = strtoull(qid, NULL, 10);			 
			 strncpy(d->qf, qfname, sizeof d->qf);
			 strncpy(d->dir, dir, sizeof d->dir);
			 d->deliver = deliver;
			 
			 gwlist_produce(items_list, d);		    
		    }
	  
	  PQclear(r);
	  return_conn(c); /* return connection to pool. */
	  
	  if (*rstop)
	       break;
	  gwthread_sleep(sleepsecs);
     } while (1);
     
     info(0, "pgsql_queue: Queue runner on [%s] shutdown, started...", dir);
     gwlist_remove_producer(items_list);


     for (i=0;i<num_threads; i++) 
	  gwthread_cancel(th_ids[i]);

     for (i=0;i<num_threads; i++) 
	  gwthread_join(th_ids[i]);    

     gwlist_destroy(items_list, NULL);
     gw_free(th_ids);

     info(0, "pgsql_queue: Queue runner on [%s] shutdown, complete...", dir);
}

/* export functions... */
MmsQueueHandlerFuncs qfuncs = {
     pgq_init_module,
     pgq_init_queue_dir,
     pgq_cleanup_module,
     pgq_queue_add,   
     pgq_queue_update,
     pgq_queue_getdata,
     pgq_queue_replacedata,
     pgq_queue_readenvelope,
     pgq_queue_run,
     pgq_queue_free_env
};
