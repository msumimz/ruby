%VALUE = type i32

define %VALUE @rbjit__bitwise_compare_eq(%VALUE %v1, %VALUE %v2) nounwind readnone alwaysinline {
  %cmp = icmp eq %VALUE %v1, %v2
  %ret = select i1 %cmp, %VALUE 2, %VALUE 0
  ret %VALUE %ret
}

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
