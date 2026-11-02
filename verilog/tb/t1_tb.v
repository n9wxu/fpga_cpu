// Testbench for t1_core: 1 MB of RAM, the console, EXIT and cycle-counter
// I/O registers (docs/io.md), and an optional lockstep check against fsim.
//
//   +image=FILE   hex bytes, one per line, loaded at +load (default 1000)
//   +trace=FILE   lockstep: one line per fsim instruction, written by
//                 t1run.py: "pc r0 .. r15 flags kind" in hex; kind 1 = MFSR,
//                 2 = a load (resync if it read I/O, where counters differ)
//   +entry=HEX    start address (default = the load address)
//   +max=N        stop after N cycles
//   +busstat      at the end, print what every bus cycle did, and why idle
//                 cycles were idle, by instruction class
//   +marker=HEX   frame marker: on each byte write here, print the cycles and
//                 instructions since the last one, and with +vram=HEX:N also
//                 write the N bytes there to frameK.hex (K = the value - 1)
//
// Prints the console output, then "T1 cycles=N insts=N exit=N".

`timescale 1ns / 1ns
`default_nettype none

module t1_tb;
    parameter EXT_X = 1;
    reg clk = 1'b0, rst = 1'b1;
    always #5 clk = ~clk;

    wire [23:0] addr;
    wire        mem_rd, mem_wr, halted, retire;
    wire [7:0]  dout, halt_code;
    wire [1:0]  sr_sel;
    reg  [7:0]  din;
    reg  [15:0] sr_in;
    reg  [23:0] reset_pc;

`ifdef T1F
    t1f_core #(.EXT_X(EXT_X)) core (.clk(clk), .ce(1'b1), .rst(rst), .reset_pc(reset_pc), .addr(addr), .mem_rd(mem_rd), .mem_wr(mem_wr),
`else
    t1_core #(.EXT_X(EXT_X)) core (.clk(clk), .rst(rst), .reset_pc(reset_pc), .addr(addr), .mem_rd(mem_rd), .mem_wr(mem_wr),
`endif
                  .dout(dout), .din(din), .sr_sel(sr_sel), .sr_in(sr_in),
                  .halted(halted), .halt_code(halt_code), .retire(retire));

    reg [7:0] ram [0:20'hFFFFF];
    reg [63:0] cycles = 0, insts = 0, maxc;
    reg [15:0] con_in_word;
    reg [7:0]  io_last;     // the last I/O read was at F0xxxx
    integer    i;
    reg [23:0] cur_pc;          // address of the instruction executing

    // memory and I/O: reads are combinational, writes at the clock edge
    wire [7:0] ram_q = ram[addr[19:0]];
    always @* begin
        din = 8'h00;
        if (addr < 24'h100000)
            din = ram_q;
        else if (addr[23:16] == 8'hF0)
            case (addr[15:0])
            16'h0004: din = cycles[7:0];
            16'h0005: din = cycles[15:8];
            16'h0006: din = cycles[23:16];
            16'h0007: din = cycles[31:24];
            16'h0008: din = 8'hFE;          // CON_IN: input has ended
            16'h0009: din = 8'hFF;
            default:  din = 8'h00;
            endcase
        case (sr_sel)
        2'd0: sr_in = cycles[15:0];
        2'd1: sr_in = cycles[31:16];
        default: sr_in = insts[15:0];
        endcase
    end

    reg [23:0] marker, vbase;
    reg [31:0] vlen;
    reg [63:0] mark_c = 0, mark_i = 0;
    reg [8*64-1:0] fname;
    integer    ff, k;
    always @(posedge clk) if (!rst && mem_wr && addr == marker) begin
        $display("T1 mark %0d cycles=%0d insts=%0d", dout, cycles - mark_c, insts - mark_i);
        mark_c = cycles;
        mark_i = insts;
        if (vlen != 0 && dout != 0) begin
            $sformat(fname, "frame%0d.hex", dout - 1);
            ff = $fopen(fname, "w");
            for (k = 0; k < vlen; k = k + 1)
                $fdisplay(ff, "%02x", ram[vbase + k]);
            $fclose(ff);
        end
    end

    // ---- bus statistics (+busstat) ----
    // busy: S_FETCH / S_P2 fetch, prefetch during execute, data. Idle:
    // the one-parcel prefetch buffer is full, or the state forbids a
    // prefetch (branches and jumps use the PC; PC-relative pair ops).
    reg        busstat = 1'b0;
    reg [63:0] b_fetch = 0, b_p2 = 0, b_pf = 0, b_data = 0, b_full [0:15], b_block [0:15], b_other = 0, b_waste = 0, b_jumps = 0, b_mul = 0;
    initial for (i = 0; i < 16; i = i + 1) begin b_full[i] = 0; b_block[i] = 0; end
    always @(posedge clk) if (!rst && busstat) begin
        if (mem_wr || mem_rd) begin
            if (core.state == 3'd0) b_fetch = b_fetch + 1;
            else if (core.state == 3'd1) b_p2 = b_p2 + 1;
            else if (core.bus_ar) b_data = b_data + 1;
            else b_pf = b_pf + 1;
        end else if (core.state == 3'd2 && core.nbn == 2'd2 && core.pf_ok) b_full[core.cls] = b_full[core.cls] + 1;
        else if (core.state == 3'd2 && !core.pf_ok) b_block[core.cls] = b_block[core.cls] + 1;
        else b_other = b_other + 1;
        if (core.jump) begin b_jumps = b_jumps + 1; b_waste = b_waste + core.nbn; end
        if (core.state == 3'd2 && core.cls == 4'd2) b_mul = b_mul + 1;
    end
    task print_busstat;
        begin
            if (busstat) begin
                $display("BUS fetch %0d p2 %0d prefetch %0d data %0d other_idle %0d jumps %0d discarded_prefetch_bytes %0d multiply_cycles %0d",
                         b_fetch, b_p2, b_pf, b_data, b_other, b_jumps, b_waste, b_mul);
                for (i = 0; i < 16; i = i + 1)
                    if (b_full[i] != 0 || b_block[i] != 0)
                        $display("BUS class %0d idle_buffer_full %0d idle_blocked %0d", i, b_full[i], b_block[i]);
            end
        end
    endtask

    // nothing may reach the bus or the register file while in reset
    always @(posedge clk) if (rst && (mem_rd !== 1'b0 || mem_wr !== 1'b0 || core.rf.we !== 1'b0))
        $display("\nT1 bus or register-file write during reset (rd=%b wr=%b we=%b)", mem_rd, mem_wr, core.rf.we);

    always @(posedge clk) if (!rst) begin
        cycles <= cycles + 1;
        if (retire) insts <= insts + 1;
        if (mem_wr) begin
            if (addr < 24'h100000)
                ram[addr[19:0]] <= dout;
            else if (addr == 24'hF00000)
                $write("%c", dout);
            else if (addr == 24'hF00002) begin
                $fflush;
                $display("\nT1 cycles=%0d insts=%0d exit=%0d", cycles, insts, dout);
                print_busstat;
                $finish;
            end else if (addr[23:16] != 8'hF0) begin
                $display("\n(bus error: write to unmapped %06x)", addr);
                $display("T1 cycles=%0d insts=%0d halt=253", cycles, insts);       // as fsim: a fault
                $finish;
            end
        end
        if (mem_rd && addr >= 24'h100000 && addr[23:16] != 8'hF0 && addr[23:16] != 8'hFF) begin
            $display("\n(bus error: read from unmapped %06x, pc near %06x)", addr, cur_pc);
            $display("T1 cycles=%0d insts=%0d halt=253", cycles, insts);           // as fsim: a fault
            $finish;
        end
        if (halted) begin
            $display("\nT1 cycles=%0d insts=%0d halt=%0d", cycles, insts, halt_code);
            $finish;
        end
        if (cycles >= maxc) begin
            $display("\nT1 cycles=%0d insts=%0d timeout", cycles, insts);
            print_busstat;
            $finish;
        end
    end

    // ---- lockstep against fsim ----
    integer    tf, n;
    reg [23:0] ret_pc, t_pc;
    reg [15:0] t_r [0:15];
    reg [3:0]  t_fl;
    reg [3:0]  t_kind;
    reg        pending = 1'b0, io_read = 1'b0;
    reg [15:0] got;
    reg        bad;

    // which instruction is executing: the core loads IR when it starts one
    always @(negedge clk) if (!rst) begin
        if (pending && tf != 0) begin
            n = $fscanf(tf, "%h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h\n", t_pc,
                        t_r[0], t_r[1], t_r[2], t_r[3], t_r[4], t_r[5], t_r[6], t_r[7],
                        t_r[8], t_r[9], t_r[10], t_r[11], t_r[12], t_r[13], t_r[14], t_r[15], t_fl, t_kind);
            if (n != 19) begin
                $display("\nT1 lockstep: trace ended after %0d instructions", insts);
                $finish;
            end
            bad = (t_pc != ret_pc) || (t_fl != {core.fN, core.fZ, core.fC, core.fV});
            for (i = 0; i < 16; i = i + 1) begin
                got = {core.rf.mem[2 * i + 1], core.rf.mem[2 * i]};
                if (got != t_r[i]) bad = 1'b1;
            end
            if (bad && (t_kind == 1 || (t_kind == 2 && io_read))) begin
                for (i = 0; i < 16; i = i + 1) begin            // counters differ from fsim's: take its values
                    core.rf.mem[2 * i] = t_r[i][7:0];
                    core.rf.mem[2 * i + 1] = t_r[i][15:8];
                end
                bad = 1'b0;
            end
            if (bad) begin
                $display("\nT1 lockstep MISMATCH after instruction %0d at %06x (trace pc %06x)", insts, ret_pc, t_pc);
                $display("  fsim: %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x flags %h",
                         t_r[0], t_r[1], t_r[2], t_r[3], t_r[4], t_r[5], t_r[6], t_r[7],
                         t_r[8], t_r[9], t_r[10], t_r[11], t_r[12], t_r[13], t_r[14], t_r[15], t_fl);
                $write("  t1:  ");
                for (i = 0; i < 16; i = i + 1) $write("%02x%02x ", core.rf.mem[2 * i + 1], core.rf.mem[2 * i]);
                $display("flags %h", {core.fN, core.fZ, core.fC, core.fV});
                $finish;
            end
            pending = 1'b0;
        end
        if (core.done || core.jump) begin
            pending = 1'b1; ret_pc = cur_pc; io_read = 1'b0;
        end
        if (mem_rd && core.bus_ar && addr[23:16] == 8'hF0) io_read = 1'b1;
        if ((core.state == 3'd0 && core.nbn == 2'd1) || (core.done && core.state_n != 3'd0))
            cur_pc = (core.pcx_n ? {4'h1, core.pcf_n[19:0]} : {{4{core.pcf_n[20]}}, core.pcf_n[19:0]}) - 24'd2;
    end

    reg [8*512-1:0] path;
    reg [23:0] load;
    initial begin
        for (i = 0; i < 32'h100000; i = i + 1) ram[i] = 8'h00;
        if (!$value$plusargs("load=%h", load)) load = 24'h001000;
        if (!$value$plusargs("max=%d", maxc)) maxc = 64'd4000000000;
        if (!$value$plusargs("marker=%h", marker)) marker = 24'hFFFFFF;
        busstat = $test$plusargs("busstat");
        if (!$value$plusargs("vram=%h", vbase)) vbase = 0;
        if (!$value$plusargs("vramlen=%d", vlen)) vlen = 0;
        if ($value$plusargs("image=%s", path)) $readmemh(path, ram, load);
        else begin $display("usage: +image=FILE"); $finish; end
        tf = 0;
        if ($value$plusargs("trace=%s", path)) tf = $fopen(path, "r");
        if (!$value$plusargs("entry=%h", reset_pc)) reset_pc = load;
        cur_pc = reset_pc;
        repeat (2) @(posedge clk);
        rst = 1'b0;
    end
endmodule

`default_nettype wire
