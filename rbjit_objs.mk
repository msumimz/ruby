rbjit_srcdir = rbjit/src
rbjit_primdir = rbjit/primitives
rbjit_objdir = rbjit/obj

RBJIT_OBJS = \
	$(rbjit_objdir)/cfgbuilder.$(OBJEXT) \
	$(rbjit_objdir)/codeduplicator.$(OBJEXT) \
	$(rbjit_objdir)/common.$(OBJEXT) \
	$(rbjit_objdir)/controlflowgraph.$(OBJEXT) \
	$(rbjit_objdir)/cooperdominatorfinder.$(OBJEXT) \
	$(rbjit_objdir)/definfo.$(OBJEXT) \
	$(rbjit_objdir)/defusechain.$(OBJEXT) \
	$(rbjit_objdir)/debugtools.$(OBJEXT) \
	$(rbjit_objdir)/dominatorfinder.$(OBJEXT) \
	$(rbjit_objdir)/domtree.$(OBJEXT) \
	$(rbjit_objdir)/idstore.$(OBJEXT) \
	$(rbjit_objdir)/inliner.$(OBJEXT) \
	$(rbjit_objdir)/ltdominatorfinder.$(OBJEXT) \
	$(rbjit_objdir)/methodinfo.$(OBJEXT) \
	$(rbjit_objdir)/mutatortester.$(OBJEXT) \
	$(rbjit_objdir)/nativecompiler.$(OBJEXT) \
	$(rbjit_objdir)/opcode.$(OBJEXT) \
	$(rbjit_objdir)/opcodefactory.$(OBJEXT) \
	$(rbjit_objdir)/opcodemultiplexer.$(OBJEXT) \
	$(rbjit_objdir)/primitivestore.$(OBJEXT) \
	$(rbjit_objdir)/recompilationmanager.$(OBJEXT) \
	$(rbjit_objdir)/rubymethod.$(OBJEXT) \
	$(rbjit_objdir)/rubyobject.$(OBJEXT) \
	$(rbjit_objdir)/runtimefunctions.$(OBJEXT) \
	$(rbjit_objdir)/setup.$(OBJEXT) \
	$(rbjit_objdir)/ssachecker.$(OBJEXT) \
	$(rbjit_objdir)/ssatranslator.$(OBJEXT) \
	$(rbjit_objdir)/typeanalyzer.$(OBJEXT) \
	$(rbjit_objdir)/typeconstraint.$(OBJEXT) \
	$(rbjit_objdir)/typecontext.$(OBJEXT) \
	$(rbjit_objdir)/variable.$(OBJEXT) \
	$(rbjit_primdir)/primitives.res
