{$(rbjit_srcdir)}.cpp.obj:
	$(ECHO) compiling $(<:\=/)
	$(Q) $(CC) $(CFLAGS) $(XCFLAGS) $(CPPFLAGS) $(RBJITFLAGS) $(COUTFLAG)$@ -c -Tp $(<:\=/)

{$(rbjit_primdir)}.rc.res:
	$(ECHO) compiling $(<:\=/)
	$(Q) $(RC) $(RCFLAGS) -I. -I$(<D) $(RFLAGS) -fo$@ $(<:\=/)
