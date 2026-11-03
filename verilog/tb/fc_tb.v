// Testbench for fc_core (the pipelined core): 1 MB of RAM behind two
// synchronous 32-bit ports (instructions and data, one clock to read data),
// the console, EXIT and cycle-counter I/O registers (docs/io.md), and the
// lockstep check against fsim that t1_tb.v has. Same plusargs as t1_tb.v,
// and:
//   +stall=N   the ports refuse a request N% of the time (tests the stalls)
//
// Prints the console output, then "T1 cycles=N insts=N exit=N" (the name
// is t1run.py's).

`timescale 1ns / 1ns
`default_nettype none

module fc_tb;
    parameter EXT_X = 1;
    reg clk = 1'b0, rst = 1'b1;
    always #5 clk = ~clk;

    wire        i_req, d_req, d_we, halted, retire;
    wire [23:2] i_addr;
    wire [23:0] d_addr;
    wire [3:0]  d_be;
    wire [31:0] d_wdata;
    reg         i_gnt = 1'b1, d_gnt = 1'b1;
    reg         i_rvalid = 1'b0, d_rvalid = 1'b0;
    reg  [31:0] i_rdata = 32'd0, d_rdata = 32'd0;
    wire [1:0]  sr_sel;
    reg  [15:0] sr_in;
    wire [7:0]  halt_code;
    reg  [23:0] reset_pc;

    fc_core #(.EXT_X(EXT_X)) core (.clk(clk), .rst(rst), .reset_pc(reset_pc),
        .i_req(i_req), .i_addr(i_addr), .i_gnt(i_gnt), .i_rvalid(i_rvalid), .i_rdata(i_rdata), .i_flush(),
        .d_req(d_req), .d_we(d_we), .d_addr(d_addr), .d_be(d_be), .d_wdata(d_wdata),
        .d_gnt(d_gnt), .d_rvalid(d_rvalid), .d_rdata(d_rdata),
        .sr_sel(sr_sel), .sr_in(sr_in), .halted(halted), .halt_code(halt_code), .retire(retire));

    reg [7:0]  ram [0:20'hFFFFF];
    reg [63:0] cycles = 0, insts = 0, maxc;
    integer    i, stallp;

    function [7:0] rd8(input [23:0] a);
        begin
            rd8 = 8'h00;
            if (a < 24'h100000) rd8 = ram[a[19:0]];
            else if (a[23:16] == 8'hF0)
                case (a[15:0])
                16'h0004: rd8 = cycles[7:0];
                16'h0005: rd8 = cycles[15:8];
                16'h0006: rd8 = cycles[23:16];
                16'h0007: rd8 = cycles[31:24];
                16'h0008: rd8 = 8'hFE;          // CON_IN: input has ended
                16'h0009: rd8 = 8'hFF;
                default:  rd8 = 8'h00;
                endcase
        end
    endfunction
    function [31:0] rd32(input [23:0] w);
        rd32 = {rd8(w + 24'd3), rd8(w + 24'd2), rd8(w + 24'd1), rd8(w)};
    endfunction

    always @* case (sr_sel)
        2'd0: sr_in = cycles[15:0];
        2'd1: sr_in = cycles[31:16];
        default: sr_in = insts[15:0];
    endcase

    function bad_addr(input [23:0] a);
        bad_addr = a >= 24'h100000 && a[23:16] != 8'hF0 && a[23:16] != 8'hFF;
    endfunction

    // the ports: a request taken at an edge is answered at the next
    reg io_read = 1'b0;
    always @(posedge clk) begin
        i_rvalid <= 1'b0; d_rvalid <= 1'b0;
        if (!rst) begin
            if (i_req && i_gnt) begin
                if (bad_addr({i_addr, 2'b00})) begin
                    $display("\n(bus error: fetch from unmapped %06x)", {i_addr, 2'b00});
                    $display("T1 cycles=%0d insts=%0d halt=253", cycles, insts);
                    $finish;
                end
                i_rvalid <= 1'b1; i_rdata <= rd32({i_addr, 2'b00});
            end
            if (d_req && d_gnt) begin
                if (bad_addr(d_addr)) begin
                    $display("\n(bus error: %s unmapped %06x)", d_we ? "write to" : "read from", d_addr);
                    $display("T1 cycles=%0d insts=%0d halt=253", cycles, insts);       // as fsim: a fault
                    $finish;
                end
                if (d_we) begin
                    for (i = 0; i < 4; i = i + 1) if (d_be[i]) begin
                        if ({d_addr[23:2], 2'b00} < 24'h100000) ram[{d_addr[19:2], 2'b00} + i] <= d_wdata[8 * i +: 8];
                        else if ({d_addr[23:2], 2'b00} + i == 24'hF00000) $write("%c", d_wdata[7:0]);
                        else if ({d_addr[23:2], 2'b00} + i == 24'hF00002) begin
                            $fflush;
                            $display("\nT1 cycles=%0d insts=%0d exit=%0d", cycles, insts, d_wdata[23:16]);
                            $finish;
                        end
                    end
                end else begin
                    d_rvalid <= 1'b1; d_rdata <= rd32({d_addr[23:2], 2'b00});
                    if (d_addr[23:16] == 8'hF0) io_read = 1'b1;
                end
            end
            if (stallp != 0) begin
                i_gnt <= ($random % 100 + 100) % 100 >= stallp;
                d_gnt <= ($random % 100 + 100) % 100 >= stallp;
            end
        end
    end

    always @(posedge clk) if (!rst) begin
        cycles <= cycles + 1;
        if (retire) insts <= insts + 1;
        if (halted) begin
            $display("\nT1 cycles=%0d insts=%0d halt=%0d", cycles, insts, halt_code);
            $finish;
        end
        if (cycles >= maxc) begin
            $display("\nT1 cycles=%0d insts=%0d timeout", cycles, insts);
            $finish;
        end
    end

    // ---- lockstep against fsim: after each instruction retires, its pc,
    // the registers and the flags
    integer    tf, n;
    reg [23:0] t_pc;
    reg [15:0] t_r [0:15];
    reg [3:0]  t_fl, t_kind;
    reg [15:0] got;
    reg        bad;
    function [15:0] reg_of(input integer r);
        reg_of = r[0] ? core.od[r >> 1] : core.ev[r >> 1];
    endfunction
    always @(negedge clk) if (!rst && retire && tf != 0) begin
        n = $fscanf(tf, "%h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h\n", t_pc,
                    t_r[0], t_r[1], t_r[2], t_r[3], t_r[4], t_r[5], t_r[6], t_r[7],
                    t_r[8], t_r[9], t_r[10], t_r[11], t_r[12], t_r[13], t_r[14], t_r[15], t_fl, t_kind);
        if (n != 19) begin
            $display("\nT1 lockstep: trace ended after %0d instructions", insts);
            $finish;
        end
        bad = (t_pc != core.ret_pc) || (t_fl != core.ret_fl);
        for (i = 0; i < 16; i = i + 1) if (reg_of(i) != t_r[i]) bad = 1'b1;
        if (bad && (t_kind == 1 || (t_kind == 2 && io_read))) begin
            for (i = 0; i < 16; i = i + 1)              // counters differ from fsim's: take its values
                if (i[0]) core.od[i >> 1] = t_r[i]; else core.ev[i >> 1] = t_r[i];
            bad = 1'b0;
        end
        if (bad) begin
            $display("\nT1 lockstep MISMATCH after instruction %0d at %06x (trace pc %06x)", insts, core.ret_pc, t_pc);
            $display("  fsim: %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x flags %h",
                     t_r[0], t_r[1], t_r[2], t_r[3], t_r[4], t_r[5], t_r[6], t_r[7],
                     t_r[8], t_r[9], t_r[10], t_r[11], t_r[12], t_r[13], t_r[14], t_r[15], t_fl);
            $write("  fc:   ");
            for (i = 0; i < 16; i = i + 1) $write("%04x ", reg_of(i));
            $display("flags %h", core.ret_fl);
            $finish;
        end
        io_read = 1'b0;
    end

    // +dbg: a line per clock of what the pipeline holds
    reg dbg = 1'b0;
    always @(posedge clk) if (dbg && !rst)
        $display("c%0d D:%b %06x %04x | X:%b %06x cls%0d pa=%08x pb=%08x ea=%06x flt=%b jmp=%b | M:%b %06x | W:%b %06x | qn=%0d ofl=%0d kill=%0d fa=%06x ir=%b/%06x rv=%b",
            cycles, core.d_valid, core.d_pc, core.d_ir, core.x_valid, core.x_pc, core.x_cls, core.pa, core.pb, core.ea,
            core.x_fault, core.x_jump, core.m_valid, core.m_pc, core.w_valid, core.w_pc, {core.fv1, core.fv0}, core.ofl, core.kill,
            {core.fa, 2'b00}, core.i_req, {core.i_addr, 2'b00}, core.i_rvalid);
    reg [8*512-1:0] path;
    reg [23:0] load;
    initial begin
        for (i = 0; i < 32'h100000; i = i + 1) ram[i] = 8'h00;
        for (i = 0; i < 8; i = i + 1) begin core.ev[i] = 16'h0000; core.od[i] = 16'h0000; end
        if (!$value$plusargs("load=%h", load)) load = 24'h001000;
        if (!$value$plusargs("max=%d", maxc)) maxc = 64'd4000000000;
        if (!$value$plusargs("stall=%d", stallp)) stallp = 0;
        dbg = $test$plusargs("dbg");
        if ($value$plusargs("image=%s", path)) $readmemh(path, ram, load);
        else begin $display("usage: +image=FILE"); $finish; end
        tf = 0;
        if ($value$plusargs("trace=%s", path)) tf = $fopen(path, "r");
        if (!$value$plusargs("entry=%h", reset_pc)) reset_pc = load;
        repeat (2) @(posedge clk);
        rst = 1'b0;
    end
endmodule

`default_nettype wire
