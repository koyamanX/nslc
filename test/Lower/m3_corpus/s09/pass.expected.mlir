module {
  nsl.module @m_pass {
    %0 = nsl.reg "i" : !nsl.bits<8>
    nsl.proc @p {
      nsl.seq {
        %1 = nsl.constant 8 : !nsl.bits<8>
        %2 = nsl.lt %0, %1 : !nsl.bits<8> -> !nsl.bits<1>
        nsl.for %0, %2, %0 : !nsl.bits<8>, !nsl.bits<1>, !nsl.bits<8> {
        }
      }
    }
  }
}

