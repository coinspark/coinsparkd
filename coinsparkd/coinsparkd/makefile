include ../linux/makefile-linux

DEPS = 
OBJS = main.o dbleveldb.o 
TRKS = bitcoin.o tracker.o transaction.o block.o blockchain.o
COMS = alloc.o utility.o net.o log.o buffer.o dbserver.o 
PRTS = coinspark.o
UNXS = linux.o

TARGET_EXE=coinsparkd

$(OUTDIR)/$(TARGET_EXE): $(OBJDIR_OBJS) $(COMDIR_OBJS) $(PRTDIR_OBJS) $(TRKDIR_OBJS) $(UNXDIR_OBJS)
	@echo " "
	@echo ">>> Linking $@"
	@$(LD) -o $@ $(UNXDIR_OBJS) $(COMDIR_OBJS) $(PRTDIR_OBJS) $(TRKDIR_OBJS) $(OBJDIR_OBJS) $(LIBS) $(EXE_FLAGS_PLAT) $(CPPFLAGS_PLAT)
	@echo " "
	@test -d $(CS_LAUNCH_ROOT) || $(mkdir_p) $(CS_LAUNCH_ROOT)	
	@test -d $(CS_LAUNCHDIR) || $(mkdir_p) $(CS_LAUNCHDIR)	
	@$(CP) $@ $(CS_LAUNCHDIR)

clean::
#	-rm $(OUTDIR)/$(TARGET_EXE)
	-rm -rf $(OUTDIR)

include $(CSDIR)/linux/makefile-rules
