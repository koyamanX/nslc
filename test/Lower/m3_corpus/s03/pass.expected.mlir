module {
  nsl.module @m_pass {
    %0 = nsl.wire "w" : !nsl.bits<1>
    %1 = nsl.reg "r" : !nsl.bits<1>
    nsl.func @clk {
      %2 = nsl.constant 0 : !nsl.bits<1>
      nsl.transfer %0, %2 : !nsl.bits<1>
      %3 = nsl.constant 0 : !nsl.bits<1>
      nsl.clocked_transfer %1, %3 : !nsl.bits<1>
    }
  }
}

