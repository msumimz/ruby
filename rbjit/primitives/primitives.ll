%VALUE = type i32

define i32 @rbjit__bitwise_compare_eq(%VALUE %v1, %VALUE %v2) nounwind readnone alwaysinline {
  %t = load %Q
  %cmp = icmp eq %VALUE %v1, %v2
  %ret = select i1 %cmp, %VALUE 2, %VALUE 0
  ret %VALUE %ret
}
