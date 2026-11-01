`timescale 1ns/1ps
// fpga_cpu on the Tang Nano 20K, first stage: the T1 core
// (verilog/rtl/t1_core.v) with 64 KB of block RAM, the console and program
// loading on the USB serial port (the onboard BL616's second channel), the
// LEDs and buttons, and the timers of docs/io.md.
//
// The PLL makes 16.875 MHz from the 27 MHz crystal, twice: cpu_clk for T1,
// and mem_clk, the same clock shifted 180 degrees, for memory, I/O and the
// UART. So those run half a period (29.6 ns) after T1's edge, by when T1's
// bus has settled (~21 ns on this part), and read data is back long before
// T1's next edge. (A falling-edge memory needs an inverted clock, and the
// LUT that inverts it made the block RAM's clock late: hold violations.)
//
// Serial (115200 8N1). While T1 is held in reset:
//   'L' a2 a1 a0 n2 n1 n0 <n bytes>   write n bytes from address a
//   'G' a2 a1 a0                      run T1 from address a
// At any time the four bytes DE AD BE EF hold T1 in reset again. While T1
// runs, bytes received are CON_IN, CON_OUT bytes are sent, and an EXIT
// write sends 0x04 and the low byte of the code (tools: tang/tools/run.py).
// Memory is 64 KB at 0x000000, repeated through the 1 MB below the I/O.
module t1tang (
    input  wire       clk,                  // 27 MHz
    input  wire       s1,                   // buttons, high when pressed
    input  wire       s2,
    input  wire       uart_rx,
    output wire       uart_tx,
    output wire [5:0] led                   // active low
);
    // ---- clock ---------------------------------------------------------------
    wire cpu_clk, mem_clk;
`ifdef SIM
    assign cpu_clk = clk;                   // the testbench drives the clock
    assign mem_clk = ~clk;
`else
    // 27 MHz x 5/8 = 16.875 MHz: PFD 3.375 MHz, VCO 16.875 x 64 = 1080 MHz
    rPLL #(
        .FCLKIN("27"),
        .IDIV_SEL(7),
        .FBDIV_SEL(4),
        .ODIV_SEL(64),
        .PSDA_SEL("1000"),                  // CLKOUTP: 8 x 22.5 = 180 degrees
        .DUTYDA_SEL("1000"),
        .DEVICE("GW2AR-18C")
    ) pll (
        .CLKIN(clk), .CLKOUT(cpu_clk), .CLKOUTP(mem_clk), .CLKOUTD(), .CLKOUTD3(), .LOCK(),
        .CLKFB(1'b0), .FBDSEL(6'd0), .IDSEL(6'd0), .ODSEL(6'd0),
        .DUTYDA(4'd0), .PSDA(4'd0), .FDLY(4'd0), .RESET(1'b0), .RESET_P(1'b0)
    );
`endif
    localparam CLK_HZ = 16875000;

    // ---- T1 -------------------------------------------------------------------
    wire [23:0] addr;
    wire        rd, wr, halted, retire;
    wire [7:0]  dout, halt_code;
    wire [1:0]  sr_sel;
    wire [7:0]  din;
    reg  [15:0] sr_in;
    reg         run = 1'b0;
    reg  [23:0] entry = 24'h001000;
    reg         t1_rst = 1'b1;
    always @(posedge cpu_clk) t1_rst <= !run;

    t1_core #(.EXT_X(0)) cpu (
        .clk(cpu_clk), .rst(t1_rst), .reset_pc(entry),
        .addr(addr), .mem_rd(rd), .mem_wr(wr), .dout(dout), .din(din),
        .sr_sel(sr_sel), .sr_in(sr_in),
        .halted(halted), .halt_code(halt_code), .retire(retire)
    );

    reg [31:0] cycles = 32'd0;
    reg [15:0] insts = 16'd0;
    always @(posedge cpu_clk)
        if (t1_rst) begin
            cycles <= 32'd0; insts <= 16'd0;
        end else begin
            cycles <= cycles + 1'b1; insts <= insts + retire;
        end
    always @*
        case (sr_sel)
            2'd0: sr_in = cycles[15:0];
            2'd1: sr_in = cycles[31:16];
            default: sr_in = insts;
        endcase

    // ---- UART (on the falling edge, with everything below) ---------------------
`ifdef SIM
    localparam BAUD_DIV = 4;                // fast serial in simulation
`else
    localparam BAUD_DIV = CLK_HZ / 115200;  // 146 (+0.3%)
`endif
    wire       rx_ok;
    wire [7:0] rx_byte;
    reg        tx_go = 1'b0;
    reg  [7:0] tx_byte = 8'd0;
    wire       tx_busy;
    uart #(.DIV(BAUD_DIV)) serial (
        .clk(mem_clk), .rx(uart_rx), .rx_ok(rx_ok), .rx_byte(rx_byte),
        .tx_go(tx_go), .tx_byte(tx_byte), .tx_busy(tx_busy), .tx(uart_tx)
    );

    // console output: a 256-byte FIFO in front of the transmitter
    reg [7:0] txq [0:255];
    reg [7:0] txq_w = 8'd0, txq_r = 8'd0;
    wire      txq_empty = txq_w == txq_r;
    wire      txq_full  = (txq_w + 1'b1) == txq_r;
    reg       txq_we = 1'b0;
    reg [7:0] txq_d = 8'd0;
    always @(posedge mem_clk) begin
        tx_go <= 1'b0;
        if (txq_we && !txq_full) begin
            txq[txq_w] <= txq_d;
            txq_w <= txq_w + 1'b1;
        end
        if (!tx_busy && !tx_go && !txq_empty) begin
            tx_byte <= txq[txq_r];
            tx_go   <= 1'b1;
            txq_r   <= txq_r + 1'b1;
        end
    end

    // ---- memory -------------------------------------------------------------------
    wire io = addr[23:16] == 8'hF0;
    reg  [7:0]  mem [0:65535];
    reg  [7:0]  mem_q = 8'd0;
    reg         ld_we = 1'b0;               // the loader writes when T1 is held
    reg  [15:0] ld_a = 16'd0;
    reg  [7:0]  ld_d = 8'd0;
    wire        m_we = run ? (wr && !io) : ld_we;
    wire [15:0] m_a  = run ? addr[15:0] : ld_a;
    wire [7:0]  m_d  = run ? dout : ld_d;
    always @(posedge mem_clk) begin
        if (m_we) mem[m_a] <= m_d;
        mem_q <= mem[m_a];
    end

    // ---- I/O registers (16 bits: low byte held, committed with the high byte;
    // a read of the low byte latches the high byte for the next read) ------------
    reg  [7:0]  lo = 8'd0;
    reg         io_sel = 1'b0;
    reg  [7:0]  io_q = 8'd0;
    assign din = io_sel ? io_q : mem_q;
    reg  [15:0] latch = 16'd0;
    reg  [15:0] latch_a = 16'hFFFF;
    reg  [7:0]  leds = 8'd0;
    reg  [8:0]  con_in = 9'h1FF;            // bit 8: empty
    reg  [15:0] frame = 16'd0;
    reg  [18:0] frame_div = 19'd0;          // 60 Hz: 281250 clocks
    reg  [9:0]  line = 10'd0;               // 750 lines a frame, as on the 720p display
    reg  [8:0]  line_div = 9'd0;            // 375 clocks a line
    reg  [1:0]  s_s0 = 2'b00, s_s = 2'b00;
    wire [15:0] wv = {dout, lo};

    function [15:0] rv;
        input [15:0] a;
        case ({a[15:1], 1'b0})
            16'h0004: rv = cycles[15:0];
            16'h0006: rv = cycles[31:16];
            16'h0008: rv = con_in[8] ? 16'hFFFF : {8'd0, con_in[7:0]};
            16'h000A: rv = frame;
            16'h000C: rv = {line, 5'd0, line >= 10'd720};
            16'h0100: rv = 16'hFFFF;
            16'h0302: rv = 16'd0;
            16'h0600: rv = {8'd0, leds};
            16'h0604: rv = {14'd0, s_s};
            default:  rv = 16'd0;
        endcase
    endfunction

    // the loader and the console share the receiver
    localparam L_IDLE = 3'd0, L_ADDR = 3'd1, L_LEN = 3'd2, L_DATA = 3'd3, L_GO = 3'd4;
    reg [2:0]  ls = L_IDLE;
    reg [1:0]  lk = 2'd0;
    reg [23:0] lv = 24'd0;
    reg [23:0] ln = 24'd0;
    reg        lgo = 1'b0;
    reg [31:0] sync = 32'd0;

    always @(posedge mem_clk) begin
        s_s0 <= {s2, s1}; s_s <= s_s0;
        txq_we <= 1'b0;
        ld_we  <= 1'b0;

        // timers
        if (frame_div == 19'd281249) begin frame_div <= 0; end else frame_div <= frame_div + 1'b1;
        if (line_div == 9'd374) begin
            line_div <= 0;
            if (line == 10'd749) line <= 0; else line <= line + 1'b1;
            if (line == 10'd719) frame <= frame + 1'b1;
        end else line_div <= line_div + 1'b1;

        // T1's I/O accesses (T1 holds a bus cycle for a whole clock); read
        // data is registered here, like the RAM's, for T1's next edge
        io_sel <= io;
        io_q   <= 8'd0;
        if (run && io && rd) begin
            if (!addr[0]) begin
                latch   <= rv(addr[15:0]);
                latch_a <= addr[15:0];
                io_q    <= rv(addr[15:0]);
                if ({addr[15:1], 1'b0} == 16'h0008) con_in[8] <= 1'b1;   // read removes it
            end else
                io_q <= (addr[15:0] == (latch_a | 16'd1)) ? latch[15:8] : rv(addr[15:0]) >> 8;
        end
        if (run && io && wr) begin
            if (!addr[0]) lo <= dout;
            else case ({addr[15:1], 1'b0})
                16'h0000: begin txq_we <= 1'b1; txq_d <= lo; end
                16'h0002: begin txq_we <= 1'b1; txq_d <= 8'h04; lgo <= 1'b1; lv <= {16'd0, lo}; end
                16'h0600: leds <= lo;
                default: ;
            endcase
        end
        if (lgo && !txq_we) begin           // EXIT: the code byte after 0x04
            txq_we <= 1'b1; txq_d <= lv[7:0]; lgo <= 1'b0;
        end

        // serial input: the sync word, the loader, or CON_IN
        if (rx_ok) begin
            sync <= {sync[23:0], rx_byte};
            if ({sync[23:0], rx_byte} == 32'hDEADBEEF) begin
                run <= 1'b0; ls <= L_IDLE;
            end else if (run)
                con_in <= {1'b0, rx_byte};
            else case (ls)
                L_IDLE: begin
                    lk <= 2'd0;
                    if (rx_byte == "L") ls <= L_ADDR;
                    if (rx_byte == "G") ls <= L_GO;
                end
                L_ADDR, L_GO: begin
                    lv <= {lv[15:0], rx_byte};
                    lk <= lk + 1'b1;
                    if (lk == 2'd2) begin
                        if (ls == L_GO) begin
                            entry <= {lv[15:0], rx_byte};
                            run   <= 1'b1;
                            ls    <= L_IDLE;
                        end else begin
                            ld_a <= {lv[7:0], rx_byte} - 1'b1;   // pre-decremented: each byte advances it
                            lk   <= 2'd0;
                            ls   <= L_LEN;
                        end
                    end
                end
                L_LEN: begin
                    ln <= {ln[15:0], rx_byte};
                    lk <= lk + 1'b1;
                    if (lk == 2'd2) ls <= ({ln[15:0], rx_byte} == 0) ? L_IDLE : L_DATA;
                end
                L_DATA: begin
                    ld_a  <= ld_a + 1'b1;
                    ld_d  <= rx_byte;
                    ld_we <= 1'b1;
                    ln    <= ln - 1'b1;
                    if (ln == 24'd1) ls <= L_IDLE;
                end
                default: ls <= L_IDLE;
            endcase
        end
    end

    // LEDs: T1's LEDS register; LED5 shows T1 halted, LED4 held in reset
    assign led = ~{halted, !run, leds[3:0]};
endmodule

// 8N1 UART, DIV clocks a bit.
module uart #(
    parameter DIV = 58
) (
    input  wire       clk,
    input  wire       rx,
    output reg        rx_ok = 1'b0,
    output reg  [7:0] rx_byte = 8'd0,
    input  wire       tx_go,
    input  wire [7:0] tx_byte,
    output wire       tx_busy,
    output reg        tx = 1'b1
);
    // receive: sample mid-bit
    reg [1:0]  rs = 2'b11;
    reg [15:0] rc = 16'd0;
    reg [3:0]  rb = 4'd0;
    reg        rbusy = 1'b0;
    reg [7:0]  rsh = 8'd0;
    always @(posedge clk) begin
        rs <= {rs[0], rx};
        rx_ok <= 1'b0;
        if (!rbusy) begin
            if (!rs[1]) begin rbusy <= 1'b1; rc <= DIV / 2; rb <= 4'd0; end
        end else if (rc != 0)
            rc <= rc - 1'b1;
        else begin
            rc <= DIV - 1;
            if (rb == 4'd0) begin
                if (rs[1]) rbusy <= 1'b0;   // false start
                rb <= 4'd1;
            end else if (rb <= 4'd8) begin
                rsh <= {rs[1], rsh[7:1]};
                rb  <= rb + 1'b1;
            end else begin
                rbusy <= 1'b0;
                if (rs[1]) begin rx_byte <= rsh; rx_ok <= 1'b1; end
            end
        end
    end

    // transmit
    reg [15:0] tc = 16'd0;
    reg [3:0]  tb = 4'd0;
    reg [9:0]  tsh = 10'h3FF;
    assign tx_busy = tb != 0;
    always @(posedge clk)
        if (tb == 0) begin
            if (tx_go) begin tsh <= {1'b1, tx_byte, 1'b0}; tb <= 4'd10; tc <= DIV - 1; tx <= 1'b0; end
        end else if (tc != 0)
            tc <= tc - 1'b1;
        else begin
            tc  <= DIV - 1;
            tsh <= {1'b1, tsh[9:1]};
            tx  <= tsh[1];
            tb  <= tb - 1'b1;
        end
endmodule
