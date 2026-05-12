module {
  nsl.declare @iface attributes {interface_clock = "clk", interface_reset = "rst"} {
    %0 = nsl.input_port "clk" : !nsl.bits<1>
    %1 = nsl.input_port "rst" : !nsl.bits<1>
    %2 = nsl.input_port "data" : !nsl.bits<1>
  }
}

