libmms = $(top_builddir)/mmlib/libmms.a
libmmsc = libmmsc.a

noinst_LIBRARIES = libmmsc.a
libmmsc_a_SOURCES = mmsc_cfg.c mms_detokenize.c mms_resolve.c mms_billing.c mms_detokenize_shell.c mms_resolve_shell.c mms_billing_shell.c

bin_PROGRAMS = mmsrelay.inst mmsproxy.inst mmsfromemail.inst mmssend.inst
mmsrelay_inst_SOURCES = mmsglobalsender.c mmsmobilesender.c mmsrelay.c
mmsrelay_inst_LDADD = $(libmmsc) $(libmms)
mmsproxy_inst_LDADD = $(libmmsc) $(libmms)
mmsfromemail_inst_LDADD = $(libmmsc) $(libmms)
mmssend_inst_LDADD = $(libmmsc) $(libmms)

EXTRA_DIST = mms_billing.h mms_detokenize_shell.h mms_billing_shell.h mms_resolve_shell.h mmsc_cfg.h mms_detokenize.h mmsrelay.h mms_resolve.h
