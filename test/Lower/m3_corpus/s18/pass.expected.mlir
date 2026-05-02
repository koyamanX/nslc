module {
  nsl.struct @hdr_t {
    nsl.field_decl "msb_field" : !nsl.bits<4>
    nsl.field_decl "mid_field" : !nsl.bits<2>
    nsl.field_decl "lsb_field" : !nsl.bits<2>
  }
}

