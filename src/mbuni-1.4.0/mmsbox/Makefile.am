libmms = $(top_builddir)/mmlib/libmms.a

bin_PROGRAMS = mmsbox.inst
mmsbox_inst_SOURCES = mmsbox.c mmsbox_cfg.c dlr.c bearerbox.c mmsbox_resolve.c mmsbox_resolve_shell.c
mmsbox_inst_LDADD = $(libmms)

EXTRA_DIST = mmsbox_cfg.h  mmsbox.h mmsbox_mt_filter.h mmsbox_resolve_shell.h mmsbox_resolve.h mmsbox_mmsc.h
