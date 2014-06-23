%VALUE = type i32

declare {i32, i1} @llvm.sadd.with.overflow.i32(i32 %a, i32 %b)
declare {i64, i1} @llvm.sadd.with.overflow.i64(i64 %a, i64 %b)

declare {i32, i1} @llvm.ssub.with.overflow.i32(i32 %a, i32 %b)
declare {i64, i1} @llvm.ssub.with.overflow.i64(i64 %a, i64 %b)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Boolean tests

define %VALUE @rbjit__test(%VALUE %v) nounwind readnone alwaysinline {
  %rtest = and %VALUE %v, xor (%VALUE 4, %VALUE -1)
  %cmp = icmp ne %VALUE %rtest, 0
  %ret = select i1 %cmp, %VALUE 2, %VALUE 0
  ret %VALUE %ret
}

define %VALUE @rbjit__test_not(%VALUE %v) nounwind readnone alwaysinline {
  %rtest = and %VALUE %v, xor (%VALUE 4, %VALUE -1)
  %cmp = icmp eq %VALUE %rtest, 0
  %ret = select i1 %cmp, %VALUE 2, %VALUE 0
  ret %VALUE %ret
}

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Bitwise arithmetics

define %VALUE @rbjit__bitwise_add(%VALUE %v1, %VALUE %v2) nounwind readnone alwaysinline {
  %ret = add %VALUE %v1, %v2
  ret %VALUE %ret
}

define %VALUE @rbjit__bitwise_add_overflow(%VALUE %v1, %VALUE %v2) nounwind readnone alwaysinline {
  %result = call {%VALUE, i1} @llvm.sadd.with.overflow.i32(%VALUE %v1, %VALUE %v2)
  %overflow = extractvalue {%VALUE, i1} %result, 1
  %ret = select i1 %overflow, %VALUE 2, %VALUE 0
  ret %VALUE %ret
}

define %VALUE @rbjit__bitwise_sub(%VALUE %v1, %VALUE %v2) nounwind readnone alwaysinline {
  %ret = sub %VALUE %v1, %v2
  ret %VALUE %ret
}

define %VALUE @rbjit__bitwise_sub_overflow(%VALUE %v1, %VALUE %v2) nounwind readnone alwaysinline {
  %result = call {%VALUE, i1} @llvm.ssub.with.overflow.i32(%VALUE %v1, %VALUE %v2)
  %overflow = extractvalue {%VALUE, i1} %result, 1
  %ret = select i1 %overflow, %VALUE 2, %VALUE 0
  ret %VALUE %ret
}

define %VALUE @rbjit__bitwise_shl(%VALUE %v1, %VALUE %v2) nounwind readnone alwaysinline {
  %ret = shl %VALUE %v1, %v2
  ret %VALUE %ret
}

define %VALUE @rbjit__bitwise_lshr(%VALUE %v1, %VALUE %v2) nounwind readnone alwaysinline {
  %ret = lshr %VALUE %v1, %v2
  ret %VALUE %ret
}

define %VALUE @rbjit__bitwise_ashr(%VALUE %v1, %VALUE %v2) nounwind readnone alwaysinline {
  %ret = ashr %VALUE %v1, %v2
  ret %VALUE %ret
}

define %VALUE @rbjit__bitwise_and(%VALUE %v1, %VALUE %v2) nounwind readnone alwaysinline {
  %ret = and %VALUE %v1, %v2
  ret %VALUE %ret
}

define %VALUE @rbjit__bitwise_or(%VALUE %v1, %VALUE %v2) nounwind readnone alwaysinline {
  %ret = or %VALUE %v1, %v2
  ret %VALUE %ret
}

define %VALUE @rbjit__bitwise_xor(%VALUE %v1, %VALUE %v2) nounwind readnone alwaysinline {
  %ret = xor %VALUE %v1, %v2
  ret %VALUE %ret
}

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Bitwise comparison

define %VALUE @rbjit__bitwise_compare_eq(%VALUE %v1, %VALUE %v2) nounwind readnone alwaysinline {
  %cmp = icmp eq %VALUE %v1, %v2
  %ret = select i1 %cmp, %VALUE 2, %VALUE 0
  ret %VALUE %ret
}

define %VALUE @rbjit__bitwise_compare_ne(%VALUE %v1, %VALUE %v2) nounwind readnone alwaysinline {
  %cmp = icmp ne %VALUE %v1, %v2
  %ret = select i1 %cmp, %VALUE 2, %VALUE 0
  ret %VALUE %ret
}

define %VALUE @rbjit__bitwise_compare_ugt(%VALUE %v1, %VALUE %v2) nounwind readnone alwaysinline {
  %cmp = icmp ugt %VALUE %v1, %v2
  %ret = select i1 %cmp, %VALUE 2, %VALUE 0
  ret %VALUE %ret
}

define %VALUE @rbjit__bitwise_compare_uge(%VALUE %v1, %VALUE %v2) nounwind readnone alwaysinline {
  %cmp = icmp uge %VALUE %v1, %v2
  %ret = select i1 %cmp, %VALUE 2, %VALUE 0
  ret %VALUE %ret
}

define %VALUE @rbjit__bitwise_compare_ult(%VALUE %v1, %VALUE %v2) nounwind readnone alwaysinline {
  %cmp = icmp ult %VALUE %v1, %v2
  %ret = select i1 %cmp, %VALUE 2, %VALUE 0
  ret %VALUE %ret
}

define %VALUE @rbjit__bitwise_compare_ule(%VALUE %v1, %VALUE %v2) nounwind readnone alwaysinline {
  %cmp = icmp ule %VALUE %v1, %v2
  %ret = select i1 %cmp, %VALUE 2, %VALUE 0
  ret %VALUE %ret
}

define %VALUE @rbjit__bitwise_compare_sgt(%VALUE %v1, %VALUE %v2) nounwind readnone alwaysinline {
  %cmp = icmp sgt %VALUE %v1, %v2
  %ret = select i1 %cmp, %VALUE 2, %VALUE 0
  ret %VALUE %ret
}

define %VALUE @rbjit__bitwise_compare_sge(%VALUE %v1, %VALUE %v2) nounwind readnone alwaysinline {
  %cmp = icmp sge %VALUE %v1, %v2
  %ret = select i1 %cmp, %VALUE 2, %VALUE 0
  ret %VALUE %ret
}

define %VALUE @rbjit__bitwise_compare_slt(%VALUE %v1, %VALUE %v2) nounwind readnone alwaysinline {
  %cmp = icmp slt %VALUE %v1, %v2
  %ret = select i1 %cmp, %VALUE 2, %VALUE 0
  ret %VALUE %ret
}

define %VALUE @rbjit__bitwise_compare_sle(%VALUE %v1, %VALUE %v2) nounwind readnone alwaysinline {
  %cmp = icmp sle %VALUE %v1, %v2
  %ret = select i1 %cmp, %VALUE 2, %VALUE 0
  ret %VALUE %ret
}

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Type tests

define %VALUE @rbjit__is_fixnum(%VALUE %v) nounwind readnone alwaysinline {
  %v1 = and %VALUE %v, 1
  %cmp = icmp eq %VALUE %v1, 1
  %ret = select i1 %cmp, %VALUE 2, %VALUE 0
  ret %VALUE %ret
}

define %VALUE @rbjit__is_symbol(%VALUE %v) nounwind readnone alwaysinline {
  %v1 = and %VALUE %v, 15
  %cmp = icmp eq %VALUE %v1, 14
  %ret = select i1 %cmp, %VALUE 2, %VALUE 0
  ret %VALUE %ret
}

define %VALUE @rbjit__is_true(%VALUE %v) nounwind readnone alwaysinline {
  %cmp = icmp eq %VALUE %v, 2
  %ret = select i1 %cmp, %VALUE 2, %VALUE 0
  ret %VALUE %ret
}

define %VALUE @rbjit__is_false(%VALUE %v) nounwind readnone alwaysinline {
  %cmp = icmp eq %VALUE %v, 0
  %ret = select i1 %cmp, %VALUE 2, %VALUE 0
  ret %VALUE %ret
}

define %VALUE @rbjit__is_nil(%VALUE %v) nounwind readnone alwaysinline {
  %cmp = icmp eq %VALUE %v, 4
  %ret = select i1 %cmp, %VALUE 2, %VALUE 0
  ret %VALUE %ret
}

define %VALUE @rbjit__is_undef(%VALUE %v) nounwind readnone alwaysinline {
  %cmp = icmp eq %VALUE %v, 6
  %ret = select i1 %cmp, %VALUE 2, %VALUE 0
  ret %VALUE %ret
}

define %VALUE @rbjit__is_embedded(%VALUE %v) nounwind readnone alwaysinline {
  %v1 = and %VALUE %v, 3
  %cmp = icmp eq %VALUE %v1, 0
  %ret = select i1 %cmp, %VALUE 0, %VALUE 2
  ret %VALUE %ret
}

define %VALUE @rbjit__class_of(%VALUE %obj) nounwind readonly alwaysinline {
  ; test if embedded object
  %lowbits = and %VALUE %obj, 3
  %cmp = icmp eq %VALUE %lowbits, 0
  br i1 %cmp, label %objptr, label %embedded

embedded:
  ret %VALUE 0

objptr:
  %rbasic = inttoptr %VALUE %obj to {%VALUE, %VALUE}*
  %pcls = getelementptr {%VALUE, %VALUE}* %rbasic, i32 0, i32 1
  %cls = load %VALUE* %pcls
  ret %VALUE %cls
}

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Type converter

define %VALUE @rbjit__fixnum_to_int(%VALUE %v) nounwind readonly alwaysinline {
  %ret = ashr %VALUE %v, 1
  ret %VALUE %ret
}

