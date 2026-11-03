`timescale 1ns / 1ns
`default_nettype none
// T1S's decoder, in two halves around a block-RAM ROM (t1s_rom_x*.vh, made
// by verilog/tb/t1sgen.py from T1F's decoder, t1_dec.v):
//
//   t1s_key     the instruction's 10-bit key: its opcode and whichever
//               other bits choose what it does
//   (the ROM)   key -> an 81-bit word: the controls, a select for each
//               field, and a mask of zero tests (reserved bits)
//   t1s_fields  the word and the parcels -> T1F's decoder outputs
//
// Together they equal t1f_dec (t1s_dec_tb.v checks every first parcel and
// many second parcels). The ROM's output register is the decode register:
// the key goes in at one clock, the decode comes out after it.

// key layout (t1sgen.py's key()):
//   00 tttt ssss   opcode t (not A, E, F), s: 0: ir[3:0]  6: ir[4]  7: ir[5]
//                  8: ir[7]  9: ir[11:8]  B: ir[4]  D: ir[8]
//   01 aaaa bbbb   MISC: ir[11:8], ir[3:0]
//   10 1 ooo 0000  ALUI: ir[10:8]
//   10 0 ooo x p b 0   E-form memory: ir[10:8], ir[0], p2[11:9] == 100, p2[8]
//   11 1 ooo ffff  REG3: ir[10:8], p2[11:8]
//   11 0 cccc 000  B/32, JAL: ir[10:7]
module t1s_key (
    input  wire [15:0] ir,
    input  wire [15:0] p2,
    output reg  [9:0]  k
);
    always @*
        case (ir[15:12])
        4'hA: k = {2'b01, ir[11:8], ir[3:0]};
        4'hE: k = ir[11] ? {3'b101, ir[10:8], 4'd0}
                         : {3'b100, ir[10:8], ir[0], p2[11:9] == 3'b100, p2[8], 1'b0};
        4'hF: k = ir[11] ? {3'b111, ir[10:8], p2[11:8]} : {3'b110, ir[10:7], 3'd0};
        4'h0: k = {2'b00, 4'h0, ir[3:0]};
        4'h6: k = {2'b00, 4'h6, 3'd0, ir[4]};
        4'h7: k = {2'b00, 4'h7, 3'd0, ir[5]};
        4'h8: k = {2'b00, 4'h8, 3'd0, ir[7]};
        4'h9: k = {2'b00, 4'h9, ir[11:8]};
        4'hB: k = {2'b00, 4'hB, 3'd0, ir[4]};
        4'hD: k = {2'b00, 4'hD, 3'd0, ir[8]};
        default: k = {2'b00, ir[15:12], 4'd0};
        endcase
endmodule

module t1s_fields (
    input  wire [80:0] w,                   // the ROM word
    input  wire [15:0] ir,
    input  wire [15:0] p2,
    output wire [3:0]  cls,
    output wire [3:0]  fn,
    output wire [3:0]  ra_r,
    output wire [3:0]  rb_r,
    output wire [3:0]  rd_r,
    output wire [3:0]  xr,
    output reg  [3:0]  sh_n,
    output wire [3:0]  cc,
    output wire        fsh,
    output wire        mp,
    output wire        b_imm,
    output wire        mld,
    output wire        sfill,
    output reg         zx,
    output wire        sh_reg,
    output wire        pstop,
    output wire        mw,
    output wire        msig,
    output reg         narrow,
    output reg         ifill,
    output wire        corr,
    output reg  [23:0] imm,
    output reg  [2:0]  pn,
    output wire [2:0]  sr,
    output wire [1:0]  msz,
    output wire [1:0]  mmode,
    output reg  [1:0]  sc,
    output wire [1:0]  shk,
    output wire [1:0]  pa_src,
    output wire [1:0]  pb_src,
    output wire [7:0]  tcode
);
    // the word (t1sgen.py's LAYOUT)
    wire [3:0] c_cls = w[3:0];
    assign fn = w[7:4];
    assign cc = w[11:8];
    assign shk = w[13:12];
    assign fsh = w[14];
    assign mp = w[15];
    assign b_imm = w[16];
    assign mld = w[17];
    assign sfill = w[18];
    assign sh_reg = w[19];
    assign pstop = w[20];
    assign mw = w[21];
    assign msig = w[22];
    assign corr = w[23];
    assign msz = w[25:24];
    assign mmode = w[27:26];
    assign pa_src = w[29:28];
    assign pb_src = w[31:30];
    wire [3:0]  s_ra = w[35:32], s_rb = w[39:36], s_rd = w[43:40], s_xr = w[47:44];
    wire [2:0]  s_pn = w[50:48];
    wire [1:0]  s_shn = w[52:51];
    wire        s_sr = w[53];
    wire [1:0]  s_tc = w[55:54];
    wire [3:0]  s_imm = w[59:56];
    wire [2:0]  s_ifill = w[62:60];
    wire [1:0]  s_nar = w[64:63], s_zx = w[66:65], s_sc = w[68:67];
    wire [11:0] mask = w[80:69];

    // zero tests: a reserved bit set is an illegal instruction
    wire [11:0] ft = {p2[9:8] == 2'b11, p2[11], p2[12], |p2[15:12], |p2[3:0], |p2[7:4],
                      ir[0], ir[4], |ir[2:0], ir[3], |ir[6:4], ir[7]};
    wire        ill = |(mask & ft);
    assign cls = ill ? 4'd12 : c_cls;
    reg  [7:0] tc;
    always @*
        case (s_tc)
        2'd1: tc = 8'hFF;
        2'd2: tc = ir[7:0];
        default: tc = 8'hFE;
        endcase
    assign tcode = ill ? 8'hFE : tc;

    function [3:0] reg4(input [3:0] s, input [15:0] i, input [15:0] p);
        case (s)
        4'd1: reg4 = i[11:8];
        4'd2: reg4 = i[7:4];
        4'd3: reg4 = i[3:0];
        4'd4: reg4 = p[15:12];
        4'd5: reg4 = {i[11:9], 1'b0};
        4'd6: reg4 = {i[6:4], 1'b0};
        4'd7: reg4 = {i[2:0], 1'b0};
        4'd8: reg4 = {i[3:1], 1'b0};
        default: reg4 = 4'd0;
        endcase
    endfunction
    assign ra_r = reg4(s_ra, ir, p2);
    assign rb_r = reg4(s_rb, ir, p2);
    assign rd_r = reg4(s_rd, ir, p2);
    assign xr   = reg4(s_xr, ir, p2);
    assign sr   = s_sr ? ir[2:0] : 3'd0;

    // B/16: offset relative to the next instruction, 2 * (off8 - 1)
    wire [8:0] boff = {ir[7], ir[7:0]} - 9'd1;
    always @* begin
        case (s_pn)
        3'd1: pn = ir[7:5];
        3'd2: pn = ir[8:6];
        3'd3: pn = ir[2:0];
        3'd4: pn = ir[3:1];
        3'd5: pn = 3'd7;
        default: pn = 3'd0;
        endcase
        case (s_shn)
        2'd1: sh_n = ir[3:0];
        2'd2: sh_n = p2[3:0];
        default: sh_n = 4'd0;
        endcase
        case (s_imm)
        4'd1:  imm = {{16{ir[7]}}, ir[7:0]};
        4'd2:  imm = {18'd0, ir[4:0], 1'b0};
        4'd3:  imm = {20'd0, ir[3:0]};
        4'd4:  imm = {16'd0, ir[6:0], 1'b0};
        4'd5:  imm = {{8{boff[8]}}, {7{boff[8]}}, boff[7:0], 1'b0};
        4'd6:  imm = 24'd1;
        4'd7:  imm = 24'd2;
        4'd8:  imm = 24'd4;
        4'd9:  imm = {{15{ir[8]}}, ir[8:0]};
        4'd10: imm = {ir[7:0], p2};
        4'd11: imm = {{8{p2[15]}}, p2};
        4'd12: imm = {8'd0, p2};
        4'd13: imm = {ir[6:0], p2, 1'b0};
        default: imm = 24'd0;
        endcase
        case (s_ifill)
        3'd1: ifill = ir[7];
        3'd2: ifill = p2[15];
        3'd3: ifill = boff[8];
        3'd4: ifill = ir[8];
        default: ifill = 1'b0;
        endcase
        case (s_nar)
        2'd1: narrow = 1'b1;
        2'd2: narrow = boff[8] == boff[7];
        default: narrow = 1'b0;
        endcase
        case (s_zx)
        2'd1: zx = p2[10];
        2'd2: zx = p2[8];
        default: zx = 1'b0;
        endcase
        case (s_sc)
        2'd1: sc = 2'd1;
        2'd2: sc = p2[9:8];
        default: sc = 2'd0;
        endcase
    end
endmodule

`default_nettype wire
