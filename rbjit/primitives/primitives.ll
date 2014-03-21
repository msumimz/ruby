define i32 @rbjit__bitwise_compare_eq(i32 %v1, i32 %v2) #0 {
  %1 = icmp eq i32 %v1, %v2
  %. = select i1 %1, i32 2, i32 0
  ret i32 %.
}

attributes #0 = { nounwind readnone alwaysinline }
