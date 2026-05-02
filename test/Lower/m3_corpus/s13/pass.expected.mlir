module {
  nsl.module @m_pass {
    %0 = nsl.wire "c" : !nsl.bits<1>
    %1 = nsl.wire "q" : !nsl.bits<1>
    nsl.alt {
      nsl.case %0 : !nsl.bits<1> {
        %2 = nsl.constant 0 : !nsl.bits<1>
        nsl.transfer %1, %2 : !nsl.bits<1>
      }
    }
    nsl.any {
      nsl.case %0 : !nsl.bits<1> {
        %2 = nsl.constant 0 : !nsl.bits<1>
        nsl.transfer %1, %2 : !nsl.bits<1>
      }
    }
  }
}

