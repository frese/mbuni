SUBDIRS = autotools doc mmlib mmsc mmsbox extras

EXTRA_DIST = VERSION GnuLICENSE KannelLICENSE bootstrap Notes 

install: 
	$(MAKE) installcheck install-recursive postinstall 

%\:install:
	@touch instance_installation
	$(MAKE) install
	./instance-install.sh $@
	

postinstall:
	./post-install.sh
        
installcheck-am:
	./svn-up2date.sh

echobindir:
	@echo $(bindir)
