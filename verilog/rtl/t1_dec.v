`timescale 1ns / 1ns
`default_nettype none
// The instruction decoder (variant A): the parcels to the instruction's
// class, fields and controls. T1's decode, unchanged, as a module: T1F and
// the pipelined core (fc_core.v) both use it. Reserved encodings come out as
// C_TRAP with tcode FE, exactly as fsim decodes them (verilog/tb/t1dec.py).
module t1f_dec #(
    parameter EXT_X = 1
) (
    input  wire [15:0] ir,
    input  wire [15:0] p2,
    output reg  [3:0] cls,
    output reg  [3:0] fn,
    output reg  [3:0] ra_r,
    output reg  [3:0] rb_r,
    output reg  [3:0] rd_r,
    output reg  [3:0] xr,
    output reg  [3:0] sh_n,
    output reg  [3:0] cc,
    output reg  fsh,
    output reg  mp,
    output reg  b_imm,
    output reg  mld,
    output reg  sfill,
    output reg  zx,
    output reg  sh_reg,
    output reg  pstop,
    output reg  mw,
    output reg  msig,
    output reg  narrow,
    output reg  ifill,
    output reg  corr,
    output reg  [23:0] imm,
    output reg  [2:0] pn,
    output reg  [2:0] sr,
    output reg  [1:0] msz,
    output reg  [1:0] mmode,
    output reg  [1:0] sc,
    output reg  [1:0] shk,
    output reg  [1:0] pa_src,
    output reg  [1:0] pb_src,
    output reg  [7:0] tcode
);
    localparam C_ALU = 4'd0, C_SHF = 4'd1, C_MUL = 4'd2, C_MEM = 4'd3, C_PAIR = 4'd4,
               C_BR = 4'd5, C_JAL = 4'd6, C_JR = 4'd7, C_JALR = 4'd8, C_MFSR = 4'd9,
               C_MTSR = 4'd10, C_NOP = 4'd11, C_TRAP = 4'd12, C_CLZ = 4'd13, C_DST = 4'd14;

    // ALUFN, isa.yaml tables.ALUFN
    localparam F_ADD = 4'd0, F_ADC = 4'd1, F_SUB = 4'd2, F_SBC = 4'd3, F_AND = 4'd4,
               F_OR = 4'd5, F_XOR = 4'd6, F_MOV = 4'd7, F_CMP = 4'd8, F_CPC = 4'd9,
               F_NEG = 4'd14, F_NOT = 4'd15;

    localparam M_D = 2'd0, M_X = 2'd1, M_PI = 2'd2, M_PD = 2'd3;


    // B/16 offset relative to the next instruction: 2*off8 - 2 = 2*(off8 - 1)
    wire [8:0] boff = {ir[7], ir[7:0]} - 9'd1;
    reg        ill;

    always @* begin
        cls = C_TRAP; tcode = 8'hFE; ill = 1'b0; fsh = 1'b0; mp = 1'b0;
        fn = F_ADD; ra_r = 4'd0; rb_r = 4'd0; rd_r = 4'd0; xr = 4'd0; sh_n = 4'd0; cc = 4'd14;
        b_imm = 1'b0; mld = 1'b0; sfill = 1'b0; zx = 1'b0; sh_reg = 1'b0; pstop = 1'b0;
        mw = 1'b0; msig = 1'b0; narrow = 1'b0; ifill = 1'b0; corr = 1'b0;
        imm = 24'd0; pn = 3'd0; sr = 3'd0;
        msz = 2'd0; mmode = M_D; sc = 2'd0; shk = 2'd0; pa_src = 2'd0; pb_src = 2'd0;
        // ill: a reserved bit or field value is set. Every form checks all of
        // its fixed bits, and pair fields (2*N) must be even.
        case (ir[15:12])
        4'h0: begin                                        // ALU2: rd = rd op rs
            fn = ir[3:0]; ra_r = ir[11:8]; rb_r = ir[7:4]; rd_r = ir[11:8];
            if (ir[3:0] >= 4'd10 && ir[3:0] <= 4'd12) begin cls = C_SHF; shk = ir[1:0] - 2'd2; sh_reg = 1'b1; end
            else if (ir[3:0] == 4'd13) cls = C_MUL;
            else cls = C_ALU;
        end
        4'h1, 4'h2, 4'h3: begin                            // ADDI / MOVI / CMPI imm8
            cls = C_ALU; fn = (ir[13:12] == 2'd1) ? F_ADD : (ir[13:12] == 2'd2) ? F_MOV : F_CMP;
            ra_r = ir[11:8]; rd_r = ir[11:8]; b_imm = 1'b1; imm = {{16{ir[7]}}, ir[7:0]};
        end
        4'h4, 4'h5: begin                                  // LDW/STW [pN + disp5*2]
            cls = C_MEM; mld = ~ir[12]; msz = 2'd1; rd_r = ir[11:8]; pn = ir[7:5];
            imm = {18'd0, ir[4:0], 1'b0}; narrow = 1'b1;
        end
        4'h6: begin                                        // LDBU/STB [pN + disp4]
            cls = C_MEM; mld = ~ir[4]; rd_r = ir[11:8]; pn = ir[7:5]; imm = {20'd0, ir[3:0]}; narrow = 1'b1;
        end
        4'h7: begin                                        // LDP/STP [pN + disp5*2]
            cls = C_MEM; mld = ~ir[5]; msz = 2'd2; rd_r = {ir[11:9], 1'b0}; pn = ir[8:6];
            imm = {18'd0, ir[4:0], 1'b0}; narrow = 1'b1;
        end
        4'h8: begin                                        // LDW/STW [sp + disp7*2]
            cls = C_MEM; mld = ~ir[7]; msz = 2'd1; rd_r = ir[11:8]; pn = 3'd7;
            imm = {16'd0, ir[6:0], 1'b0}; narrow = 1'b1;
        end
        4'h9: begin                                        // Bcc off8 (cc 15 is reserved)
            cls = (ir[11:8] == 4'd15) ? C_TRAP : C_BR; cc = ir[11:8];
            imm = {{8{boff[8]}}, {7{boff[8]}}, boff[7], boff[6:0], 1'b0};
            ifill = boff[8]; narrow = (boff[8] == boff[7]);
        end
        4'hA: case (ir[11:8])                              // MISC
            4'h0: begin cls = C_JR; pn = ir[2:0]; ill = |ir[7:3]; end
            4'h1: begin cls = C_JALR; pn = ir[2:0]; ill = |ir[7:3]; end
            4'h2: tcode = ir[7:0];                         // SYS
            4'h3: if (ir[7:0] == 8'h00) tcode = 8'hFF;     // RETI: no interrupts yet
                  else if (ir[7:0] == 8'h01 || ir[7:0] == 8'h02) cls = C_NOP;   // SYNC, NOP
            4'h5: begin cls = C_PAIR; rd_r = {ir[6:4], 1'b0}; ra_r = {ir[2:0], 1'b0}; pb_src = 2'd3; ill = ir[7] | ir[3]; end  // MOVP
            4'h6: begin cls = C_PAIR; rd_r = {ir[6:4], 1'b0}; ra_r = {ir[6:4], 1'b0}; pb_src = 2'd2; xr = ir[3:0]; ill = ir[7]; end  // ADDP
            4'h7, 4'h8, 4'h9: begin                        // SHLI / SHRI / SARI imm4
                cls = C_SHF; shk = ir[9:8] + 2'd1; sh_n = ir[3:0]; ra_r = ir[7:4]; rd_r = ir[7:4];
            end
            4'hB, 4'hC, 4'hD, 4'hE: begin                  // LDW [pN]+, STW -[pN], LDBU [pN]+, STB [pN]+
                cls = C_MEM; mld = ir[8]; mmode = (ir[11:8] == 4'hC) ? M_PD : M_PI;
                msz = {1'b0, ir[11:8] == 4'hB || ir[11:8] == 4'hC}; imm = msz[0] ? 24'd2 : 24'd1;
                rd_r = ir[7:4]; pn = ir[2:0]; ill = ir[3];
            end
            4'hF: begin cls = ir[3] ? C_MTSR : C_MFSR; rd_r = ir[7:4]; sr = ir[2:0]; end
            default: ;
            endcase
        4'hB: begin                                        // LDW/STW [pN + rX*2]
            cls = C_MEM; mld = ~ir[4]; msz = 2'd1; mmode = M_X; rd_r = ir[11:8]; pn = ir[7:5]; xr = ir[3:0]; sc = 2'd1;
        end
        4'hC: begin                                        // ADDPI pD, imm9
            cls = C_PAIR; rd_r = {ir[11:9], 1'b0}; ra_r = {ir[11:9], 1'b0};
            imm = {{15{ir[8]}}, ir[8:0]}; ifill = ir[8]; narrow = 1'b1; pstop = 1'b1;
        end
        4'hD: begin                                        // LEA pD, [abs24] / LEAPC
            cls = C_PAIR; rd_r = {ir[11:9], 1'b0}; imm = {ir[7:0], p2};
            if (ir[8]) begin pa_src = 2'd2; ifill = ir[7]; corr = 1'b1; end
            else pa_src = 2'd1;
        end
        4'hE: if (!ir[11]) begin                           // MEM, 32-bit
            rd_r = ir[7:4]; pn = ir[3:1]; ra_r = {ir[3:1], 1'b0};
            imm = {{8{p2[15]}}, p2}; ifill = p2[15];
            ill = ir[4] && (ir[10:8] == 3'd3 || ir[10:8] >= 3'd6);        // LDP, STP, LEA name a pair
            if (ir[10:8] == 3'd7) begin                    // LEA pD, [pN + disp16] / [pN + rX*s]
                cls = C_PAIR;
                if (ir[0]) begin
                    pb_src = 2'd2; xr = p2[15:12]; sc = p2[9:8]; zx = p2[10];
                    ill = ill | p2[11] | (p2[9:8] == 2'd3) | (|p2[7:0]);  // no pi/pd, scale code 3 reserved
                end
            end else begin
                cls = C_MEM; mld = ~ir[10]; sfill = (ir[10:8] == 3'd1);
                msz = ir[10] ? ir[9:8] : (ir[9:8] == 2'd3) ? 2'd2 : {1'b0, ir[9]};
                if (ir[0]) begin
                    if (p2[11:8] == 4'h8 || p2[11:8] == 4'h9) begin
                        mmode = p2[8] ? M_PD : M_PI;
                        imm = (msz == 2'd0) ? 24'd1 : (msz == 2'd1) ? 24'd2 : 24'd4; ifill = 1'b0;
                        ill = ill | (|p2[15:12]) | (|p2[7:0]);
                    end else begin
                        mmode = M_X; xr = p2[15:12]; sc = p2[9:8]; zx = p2[10];
                        ill = ill | p2[11] | (p2[9:8] == 2'd3) | (|p2[7:0]);
                    end
                end
            end
        end else begin                                     // ALUI: rd = rs op imm16
            rd_r = ir[7:4]; ra_r = ir[3:0]; b_imm = 1'b1; imm = {8'd0, p2};
            case (ir[10:8])
            3'd0: begin cls = C_ALU; fn = F_ADD; end
            3'd1: begin cls = C_ALU; fn = F_AND; end
            3'd2: begin cls = C_ALU; fn = F_OR;  end
            3'd3: begin cls = C_ALU; fn = F_XOR; end
            3'd4: begin cls = C_ALU; fn = F_CMP; ill = |ir[7:4]; end
            3'd5: begin cls = C_ALU; fn = F_MOV; ill = |ir[3:0]; end
            3'd6: begin cls = C_PAIR; imm = {{8{p2[15]}}, p2}; ifill = p2[15]; ill = ir[4] | ir[0]; end   // ADDPI pD, pS, imm16
            default: ;
            endcase
        end
        4'hF: if (!ir[11]) begin                           // Bcc off23 / JAL: 2*off23, then -4
            cls = (ir[10:7] == 4'd15) ? C_JAL : C_BR; cc = ir[10:7];
            imm = {ir[6:0], p2, 1'b0}; corr = 1'b1;
        end else begin                                     // REG3
            rd_r = ir[7:4]; ra_r = ir[3:0]; rb_r = p2[15:12];
            case (ir[10:8])
            3'd0: begin
                fn = p2[11:8]; ill = |p2[7:0];
                if (p2[11:8] >= 4'd10 && p2[11:8] <= 4'd12) begin cls = C_SHF; shk = p2[9:8] - 2'd2; sh_reg = 1'b1; end
                else if (p2[11:8] == 4'd13) cls = C_MUL;
                else begin
                    cls = C_ALU;
                    if (p2[11:8] == F_MOV || p2[11:8] == F_NEG || p2[11:8] == F_NOT) begin rb_r = ir[3:0]; ill = ill | (|p2[15:12]); end
                    if (p2[11:8] == F_CMP || p2[11:8] == F_CPC) ill = ill | (|ir[7:4]);
                end
            end
            3'd1: if (p2[11:8] <= 4'd2) begin cls = C_SHF; shk = p2[9:8]; sh_n = p2[3:0]; ill = (|p2[15:12]) | (|p2[7:4]); end
            3'd2: if (p2[11:8] <= 4'd2) begin              // ADDP / ADDPU / ADDPP
                cls = C_PAIR; ill = ir[4] | ir[0] | (|p2[7:0]);
                if (p2[9]) begin pb_src = 2'd1; ill = ill | p2[12]; end
                else begin pb_src = 2'd2; xr = p2[15:12]; zx = p2[8]; end
            end
            3'd3: if (p2[11:10] == 2'd0 && p2[9:8] != 2'd3) begin    // MULH, MULHU, MULPU
                cls = C_MUL; mw = 1'b1; msig = ~p2[9] & ~p2[8]; sfill = ~p2[9] & ~p2[8]; ill = |p2[7:0];
                if (p2[9]) begin mp = 1'b1; ill = ill | ir[4]; if (!EXT_X) cls = C_TRAP; end
            end
            3'd4: if (EXT_X && p2[11:9] == 3'd0) begin     // FSHR, FSHL
                cls = C_SHF; fsh = 1'b1; sh_reg = 1'b1; shk = {1'b0, ~p2[8]}; ill = (|p2[7:0]) | ir[0];
            end
            3'd5: if (EXT_X && p2[11:9] == 3'd0) begin     // CLZ, DSTEP
                if (p2[8]) begin cls = C_DST; ill = (|p2[15:12]) | (|p2[7:0]) | ir[4] | ir[0]; end
                else begin cls = C_CLZ; ill = (|p2[15:12]) | (|p2[7:0]); end
            end
            default: ;
            endcase
        end
        default: ;
        endcase
        if (ill) begin cls = C_TRAP; tcode = 8'hFE; end
    end
endmodule

`default_nettype wire
