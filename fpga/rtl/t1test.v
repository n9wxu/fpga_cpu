`timescale 1ns/1ps
// T1 on its own, clocked exactly as in system.v (a gated 50 MHz clock, one
// edge per five cycles, its bus registered two cycles after its edge) but
// with 4 KB of block RAM at 0x1000 instead of the SRAM, and no video. It
// runs the program in build/t1test.hex (sim/progs/t1check.c), which shows
// a checksum on the 7-segment display; the JTAG status word has it too.
// T1 starts a moment after configuration; BTN0 restarts it.
module t1test #(
    parameter IMAGE = "t1test.hex",
    parameter GATED = 1                     // 1: BUFGCE as in system.v; 0: a free-running clock
) (
    input  wire       clk,                  // 50 MHz
    input  wire [3:0] btn,
    output reg  [7:0] led = 8'd0,
    output reg  [3:0] an  = 4'b1111,
    output reg  [7:0] seg = 8'hFF
);
    wire clk50, pclk, locked;
    clkgen clocks (.clk_in(clk), .clk50(clk50), .clk_pix(pclk), .locked(locked));

    // ---- T1's clock ------------------------------------------------------------
    // GATED = 1: periods of five 50 MHz cycles, T1's edge ends each one, and
    // its bus is registered at the edge e2 (memclk = clk50, lat once a period).
    // GATED = 0: T1 runs on clk50 / 8 (6.25 MHz) through a global buffer, and
    // the RAM on that clock's falling edge (memclk = ~cpu_clk, lat always).
    reg [2:0] ph = 3'd0;
    reg       cpu_ce = 1'b0;
    always @(posedge clk50) begin
        ph     <= (ph == 3'd4) ? 3'd0 : ph + 1'b1;
        cpu_ce <= ph == 3'd3;               // high in the last cycle
    end
    wire cpu_clk, memclk, lat;
    generate if (GATED) begin : g
`ifdef SIM
        clkgate gate (.clk(clk), .ce(cpu_ce), .gclk(cpu_clk));
`else
        clkgate gate (.clk(clk50), .ce(cpu_ce), .gclk(cpu_clk));
`endif
        assign memclk = clk50;
        assign lat = ph == 3'd1;
    end else begin : f
        reg [2:0] div = 3'd0;
        always @(posedge clk50) div <= div + 1'b1;
`ifdef SIM
        assign cpu_clk = div[2];
        assign memclk = ~div[2];
`else
        BUFG bc (.I(div[2]), .O(cpu_clk));
        assign memclk = ~cpu_clk;
`endif
        assign lat = 1'b1;
    end endgenerate

    // run a moment after configuration, again after BTN0
    reg [1:0]  b0 = 2'b00;
    reg [19:0] hold = 20'hFFFFF;
    reg        t1_rst = 1'b1;
    always @(posedge clk50) begin
        b0 <= {b0[0], btn[0]};
        if (b0[1]) hold <= 20'hFFFFF;
        else if (hold != 0) hold <= hold - 1'b1;
    end
    generate if (GATED) begin : rg
        always @(posedge clk50) if (cpu_ce) t1_rst <= hold != 0;
    end else begin : rf
        always @(posedge cpu_clk) t1_rst <= hold != 0;
    end endgenerate

    wire [23:0] addr;
    wire        rd, wr, halted, retire;
    wire [7:0]  dout, halt_code;
    wire [1:0]  sr_sel;
    reg  [7:0]  din;
    t1_core #(.EXT_X(0)) cpu (
        .clk(cpu_clk), .rst(t1_rst), .reset_pc(24'h001000),
        .addr(addr), .mem_rd(rd), .mem_wr(wr), .dout(dout), .din(din),
        .sr_sel(sr_sel), .sr_in(16'd0),
        .halted(halted), .halt_code(halt_code), .retire(retire)
    );

    // ---- 4 KB of RAM at 0x1000 (aliased through the low 64 KB) ---------------
    // (a plain synchronous RAM, so XST makes it block RAM)
    wire io = addr[23:16] == 8'hF0;
    reg [7:0] ram [0:4095];
    initial $readmemh(IMAGE, ram);
    reg [7:0] ram_q = 8'd0;
    always @(posedge memclk)
        if (lat) begin
            if (wr && !io) ram[addr[11:0]] <= dout;
            ram_q <= ram[addr[11:0]];
        end
    reg was_io = 1'b0;
    always @(posedge memclk) if (lat) was_io <= io;
    always @* din = was_io ? 8'd0 : ram_q;

    reg [15:0] seg7 = 16'h0000;
    always @(posedge memclk)
        if (lat && wr && io) begin
            if (addr[15:0] == 16'h0600) led <= dout;
            if (addr[15:0] == 16'h0606) seg7[7:0] <= dout;
            if (addr[15:0] == 16'h0607) seg7[15:8] <= dout;
        end

    // ---- display and JTAG status --------------------------------------------
    reg [17:0] refresh = 18'd0;
    always @(posedge clk50) refresh <= refresh + 1'b1;
    wire [1:0] dsel = refresh[17:16];
    wire [6:0] glyph;
    seg7 dec (.digit(seg7[4*dsel +: 4]), .glyph(glyph));
    always @(posedge clk50) begin
        an  <= ~(4'b0001 << dsel);
        seg <= {1'b1, ~glyph};
    end

    jtagldr ldr (
        .clk(clk50), .status({1'b0, halted, !t1_rst, 5'd0, halt_code, seg7}),
        .run(), .entry(), .ldr_req(), .ldr_rd(), .ldr_q(32'd0), .trace_rd(),
        .ldr_addr(), .ldr_data(), .ldr_done(1'b0)
    );
endmodule
