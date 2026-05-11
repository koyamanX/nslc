module {
  nsl.declare @d {
    %0 = nsl.input_port "a" : !nsl.bits<1>
    %1 = nsl.output_port "b" : !nsl.bits<1>
  }
}

