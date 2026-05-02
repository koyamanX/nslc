module {
  nsl.module @m_pass {
    %0 = nsl.wire "a" : !nsl.bits<8>
    %1 = nsl.reg "q" : !nsl.bits<1>
  }
}

