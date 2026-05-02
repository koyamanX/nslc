module {
  nsl.module @m_pass {
    %0 = nsl.wire "c" : !nsl.bits<1>
    nsl.proc @p {
      nsl.seq {
        nsl.while %0 : !nsl.bits<1> {
        }
      }
    }
  }
}

