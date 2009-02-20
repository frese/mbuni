/*
 * Mbuni - Open  Source MMS Gateway 
 * 
 * MMS Relay, implements message routing
 * 
 * Copyright (C) 2003 - 2005, Digital Solutions Ltd. - http://www.dsmagic.com
 *
 * Paul Bagyenda <bagyenda@dsmagic.com>
 * 
 * This program is free software, distributed under the terms of
 * the GNU General Public License, with a few exceptions granted (see LICENSE)
 */
#include "mmsrelay.h"

static mCfg *cfg;
MmscSettings *settings;
List *proxyrelays;


static int rstop = 0; /* Set to 1 to stop relay. */
static void quit_now(int notused)
{
     rstop = 1;
}

/* manage the SIGHUP signal */
static void relog_now(int notused)
{
     warning(0, "SIGHUP received, catching and re-opening logs");
     log_reopen();
     alog_reopen();
}

int main(int argc, char *argv[])
{
     int cfidx;
     Octstr *fname;

     long qthread = 0;

     mms_lib_init();

     srandom(time(NULL));

     cfidx = get_and_set_debugs(argc, argv, NULL);

     if (argv[cfidx] == NULL)
	  fname = octstr_imm("mbuni.conf");
     else 
	  fname = octstr_create(argv[cfidx]);

     cfg = mms_cfg_read(fname);
     
     if (cfg == NULL)
	  panic(0, "Couldn't read configuration  from '%s'.", octstr_get_cstr(fname));

     octstr_destroy(fname);

     info(0, "----------------------------------------");
     info(0, " " MM_NAME " MMSC Relay  version %s starting", MMSC_VERSION);
     
     
     settings = mms_load_mmsc_settings(cfg,&proxyrelays);          

     mms_cfg_destroy(cfg);
     
     if (!settings) 
	  panic(0, "No global MMSC configuration!");

#if 0
     mms_start_profile_engine(octstr_get_cstr(settings->ua_profile_cache_dir));
#endif 

     signal(SIGHUP, relog_now);
     signal(SIGTERM, quit_now);
     signal(SIGPIPE,SIG_IGN); /* Ignore pipe errors. They kill us sometimes for nothing*/


     /* Start global queue runner. */
     info(0, "Starting Global Queue Runner...");
     qthread = gwthread_create((gwthread_func_t *)mbuni_global_queue_runner, &rstop);


     /* Start the local queue runner. */
     info(0, "Starting Local Queue Runner...");
     mbuni_mm1_queue_runner(&rstop);

     gwthread_cancel(qthread); /* force it to die if not yet dead. */
     
     /* It terminates, so start dying... */
     info(0, "Stopping profile engine...");
     mms_stop_profile_engine(); /* Stop profile stuff. */

     sleep(2); /* Wait for them to die. */
     info(0, "Final cleanup...");
     mms_cleanup_mmsc_settings(settings);

     info(0, "Queue runners shutdown, cleanup commenced...");
     gwthread_join(qthread); /* Wait for it to die... */

     info(0, "Shutdown complete...");
     mms_lib_shutdown();
     return 0;
};




