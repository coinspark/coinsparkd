ifeq ($(CSDIR),)
    CSDIR = $(CURDIR)
endif

include linux/makefile-linux

#COMPONENTS_DIRS = coinsparkd 
#CS_PRODUCTS = coinsparkd 

#CS_FULL = coinsparkd_full

all:
	
#	@test -h $(CS_FULLDIR) && COMPONENTS_DIRS+=` coinsparkd_full`;echo $(CS_FULL);echo $(COMPONENTS_DIRS)
	
	@test -d $(CS_LAUNCH_ROOT) || $(mkdir_p) $(CS_LAUNCH_ROOT)	
	
#	@if [ -h $(CS_FULLDIR) ]; \
#	then \
#	    COMPONENTS_DIRS +=coinsparkd_full;echo $(COMPONENTS_DIRS); \
#	fi;

	$(MAKE) -C $(CS_LEVELDBDIR)
	
	@-for dir in $(COMPONENTS_DIRS); do \
		$(MAKE) -C $$dir; \
	done;

	@rm -rf $(TEMP_BLD_DIR)
#	@test -d $(CSDIR)/../coinspark_data || $(mkdir_p) $(CSDIR)/../coinspark_data
	@test -d $(TEMP_BLD_DIR) || $(mkdir_p) $(TEMP_BLD_DIR)
	@test -d $(TEMP_BLD_DIR)/$(CS_HOME_NAME) || $(mkdir_p) $(TEMP_BLD_DIR)/$(CS_HOME_NAME)

	@test -d $(TEMP_BLD_DIR)/$(CS_HOME_NAME)/bin || $(mkdir_p) $(TEMP_BLD_DIR)/$(CS_HOME_NAME)/bin
	
	@cp $(LAUNCHDIR_PRODS) $(TEMP_BLD_DIR)/$(CS_HOME_NAME)/bin
	@chmod 775 $(TEMP_BLD_DIR)/$(CS_HOME_NAME)/bin/*
	@test -d $(BINDIR) || $(mkdir_p) $(BINDIR)
	@rm -f $(BINDIR)/*
	@cp $(LAUNCHDIR_PRODS) $(BINDIR)/

clean:
	$(MAKE) -C $(CS_LEVELDBDIR) clean
	@-for dir in $(COMPONENTS_DIRS); do \
		$(MAKE) -C $$dir clean; \
	done;
	
	-rm -rf $(UNXDIR)
	-rm -rf $(PRTDIR)
	-rm -rf $(COMDIR)
	-rm -rf $(TRKDIR)
	
	
	-rm $(LAUNCHDIR_PRODS)
	-rm -rf $(TEMP_BLD_DIR)
	-rm -rf $(BINDIR)
