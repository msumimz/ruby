{$(rbjit_srcdir)}.cpp.obj:
	$(ECHO) compiling $(<:\=/)
	$(Q) $(CC) $(CFLAGS) $(XCFLAGS) $(CPPFLAGS) $(RBJITFLAGS) $(COUTFLAG)$@ -c -Tp $(<:\=/)
