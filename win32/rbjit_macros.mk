RBJIT_CPPFLAGS = /wd4355 /I./rbjit /I./rbjit/include /I$(LLVM_PREFIX)/include /DNDEBUG /D_SCL_SECURE_NO_WARNINGS=1 /DRBJIT_ENABLED /GR /EHsc /D_SECURE_SCL=0 /D_HAS_ITERATOR_DEBUGGING=0 /D_ITERATOR_DEBUG_LEVEL=0 /Ob2 /Oi

RBJIT_LDFALGS = /libpath:$(LLVM_PREFIX)/lib /DYNAMICBASE /NXCOMPAT /SAFESEH

RBJIT_LIBS = LLVMipo.lib LLVMBitReader.lib LLVMX86Disassembler.lib LLVMX86AsmParser.lib LLVMX86CodeGen.lib LLVMSelectionDAG.lib LLVMAsmPrinter.lib LLVMMCParser.lib LLVMX86Desc.lib LLVMX86Info.lib LLVMX86AsmPrinter.lib LLVMX86Utils.lib LLVMJIT.lib LLVMRuntimeDyld.lib LLVMExecutionEngine.lib LLVMCodeGen.lib LLVMObjCARCOpts.lib LLVMScalarOpts.lib LLVMInstCombine.lib LLVMTransformUtils.lib LLVMipa.lib LLVMAnalysis.lib LLVMTarget.lib LLVMMC.lib LLVMObject.lib LLVMCore.lib LLVMSupport.lib

RBJIT_ARFLAGS =
