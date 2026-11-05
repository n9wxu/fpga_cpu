`timescale 1ns/1ps
// SST39VF1601C (16 Mbit, 1M x 16, 70 ns) for simulation, from Microchip's
// data sheet (S71380): reads, the JEDEC command set (555H/2AAH on A10-A0),
// Word-Program, Sector-, Block- and Chip-Erase, Software ID, and the status
// a read returns while a program or erase runs (DQ7 Data#, DQ6 and DQ2
// toggling). Its timing is checked; a violation prints "sst39vf: ERROR".
//
//   read:  data valid TAA/TCE 70 ns after the address and CE#, TOE 35 ns
//          after OE#; X before that. The outputs go on as soon as CE# and
//          OE# are low (TCLZ, TOLZ: 0) and off 20 ns after either rises
//          (TCHZ, TOHZ), so a controller that does not wait for them sees X.
//   write: WE#-controlled (CE# low first): the address taken at WE#'s fall,
//          the data at its rise; WE# low at least TWP 40 ns, the address
//          held TAH 30 ns past the fall, the data set up TDS 30 ns before
//          the rise.
// TBP, TSE and TSCE (program, sector/block erase, chip erase) are
// parameters, shorter than the part's for simulation.
// +flash=FILE preloads 16-bit words ($readmemh, word 0 up); erased is FFFF.
module sst39vf #(
    parameter TBP  = 10000,                 // Word-Program, ns (part: 10 us)
    parameter TSE  = 200000,                // Sector-/Block-Erase, ns (part: 25 ms)
    parameter TSCE = 400000                 // Chip-Erase, ns (part: 50 ms)
) (
    input  wire [19:0] a,
    inout  wire [15:0] dq,
    input  wire        ce_n,
    input  wire        oe_n,
    input  wire        we_n
);
    reg [15:0] mem [0:1048575];
    integer i;
    reg [1023:0] image;
    initial begin
        for (i = 0; i < 1048576; i = i + 1) mem[i] = 16'hFFFF;
        if ($value$plusargs("flash=%s", image)) $readmemh(image, mem);
    end

    // ---- the command state
    localparam S_READ = 0, S_C1 = 1, S_C2 = 2, S_PROG = 3, S_E1 = 4, S_E2 = 5, S_E3 = 6, S_ID = 7;
    integer st = S_READ, id_st = 0;         // id_st: in Software ID (1)
    reg     busy = 1'b0, erasing = 1'b0;
    reg [15:0] last_w = 16'd0;              // a program's data (DQ7 Data#)
    reg     t6 = 1'b0, t2 = 1'b0;           // the toggle bits

    // ---- reads
    wire       rd_en = !ce_n && !oe_n && we_n;
    reg  [19:0] a_d; always @* a_d <= #70 a;            // the address, settled 70 ns
    reg         ce_d; always @* ce_d <= #70 !ce_n;
    reg         oe_d; always @* oe_d <= #35 !oe_n;
    wire        settled = a_d == a && ce_d && oe_d && !ce_n && !oe_n;
    // busy: DQ7 Data# (a program; 0 erasing), DQ6 toggling, DQ2 toggling (erasing)
    wire [15:0] st_val = {8'h00, erasing ? 1'b0 : ~last_w[7], t6, 3'b000, t2, 2'b00};
    wire [15:0] rd_val = busy ? st_val : id_st ? (a[0] ? 16'h234B : 16'h00BF) : mem[a];
    wire #(0, 20) drive = rd_en;            // on at once, off 20 ns after CE# or OE# rises
    assign dq = drive ? (settled ? rd_val : 16'hxxxx) : 16'hzzzz;
    // the toggle bits move on each read while busy (at its CE# or OE# fall)
    always @(posedge rd_en) if (busy) begin t6 <= ~t6; if (erasing) t2 <= ~t2; end

    // ---- writes
    wire        w_act = !ce_n && !we_n;
    real        t_fall, t_a, t_d;
    reg  [19:0] wa;
    always @(a)  t_a = $realtime;
    always @(dq) t_d = $realtime;
    always @(posedge w_act) if ($time > 0) begin
        t_fall = $realtime; wa = a;
        if (!oe_n) begin $display("sst39vf: ERROR write with OE# low at %0t", $time); end
    end
    always @(a) if (w_act && $realtime - t_fall < 30.0)
        $display("sst39vf: ERROR address changed %0.1f ns after WE# fell (TAH 30) at %0t", $realtime - t_fall, $time);
    always @(negedge w_act) if ($time > 0) begin
        if ($realtime - t_fall < 40.0)
            $display("sst39vf: ERROR WE# low %0.1f ns (TWP 40) at %0t", $realtime - t_fall, $time);
        if ($realtime - t_d < 30.0)
            $display("sst39vf: ERROR data set up %0.1f ns (TDS 30) at %0t", $realtime - t_d, $time);
        if (dq === 16'hxxxx || ^dq[7:0] === 1'bx)
            $display("sst39vf: ERROR data X at %0t", $time);
        write(wa, dq);
    end

    task write(input [19:0] wa, input [15:0] wd);
        begin
            if (busy) begin
                // only Erase-Suspend is taken while busy (not modelled)
            end else if (wd[7:0] == 8'hF0 && (st == S_READ || st == S_C1)) begin
                id_st = 0; st = S_READ;
            end else case (st)
                S_READ, S_ID:
                    st = (wa[10:0] == 11'h555 && wd[7:0] == 8'hAA) ? S_C1 : st;
                S_C1:
                    st = (wa[10:0] == 11'h2AA && wd[7:0] == 8'h55) ? S_C2 : S_READ;
                S_C2:
                    if (wa[10:0] != 11'h555) st = S_READ;
                    else case (wd[7:0])
                        8'hA0: st = S_PROG;
                        8'h80: st = S_E1;
                        8'h90: begin id_st = 1; st = S_READ; end
                        8'hF0: begin id_st = 0; st = S_READ; end
                        default: st = S_READ;
                    endcase
                S_PROG: begin
                    st = S_READ; last_w = wd;
                    run_prog(wa, wd);
                end
                S_E1: st = (wa[10:0] == 11'h555 && wd[7:0] == 8'hAA) ? S_E2 : S_READ;
                S_E2: st = (wa[10:0] == 11'h2AA && wd[7:0] == 8'h55) ? S_E3 : S_READ;
                S_E3: begin
                    st = S_READ;
                    if (wd[7:0] == 8'h50) run_erase(wa & 20'hFF800, 2048, TSE);          // a 2 KWord sector
                    else if (wd[7:0] == 8'h30) run_erase(wa & 20'hF8000, 32768, TSE);    // a 32 KWord block (uniform here)
                    else if (wd[7:0] == 8'h10 && wa[10:0] == 11'h555) run_erase(0, 1048576, TSCE);
                end
                default: st = S_READ;
            endcase
        end
    endtask

    // program and erase take their time in the background (busy)
    reg [19:0] op_a; integer op_n; reg [15:0] op_d; integer op_t; reg op_erase;
    event go;
    task run_prog(input [19:0] pa, input [15:0] pd);
        begin op_a = pa; op_d = pd; op_n = 1; op_t = TBP; op_erase = 0; busy = 1; erasing = 0; -> go; end
    endtask
    task run_erase(input [19:0] ea, input integer n, input integer t);
        begin op_a = ea; op_n = n; op_t = t; op_erase = 1; busy = 1; erasing = 1; -> go; end
    endtask
    always @(go) begin
        #(op_t);
        if (op_erase) for (i = 0; i < op_n; i = i + 1) mem[op_a + i] = 16'hFFFF;
        else mem[op_a] = mem[op_a] & op_d;
        busy = 0; erasing = 0;
    end
endmodule
