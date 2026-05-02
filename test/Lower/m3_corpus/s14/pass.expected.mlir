module {
  nsl.module @m_pass {
    %0 = nsl.wire "c" : !nsl.bits<1>
    %1 = nsl.wire "a" : !nsl.bits<1>
    %2 = nsl.wire "b" : !nsl.bits<1>
    %3 = nsl.reg "q" : !nsl.bits<1>
  }
}

