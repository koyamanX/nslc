module {
  nsl.declare @iface {
    %0 = nsl.input_port "clk" : !nsl.bits<1>
    %1 = nsl.input_port "rst" : !nsl.bits<1>
    %2 = nsl.input_port "data" : !nsl.bits<1>
  }
}

