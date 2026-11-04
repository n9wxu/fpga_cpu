`timescale 1ns/1ps
// A mock CPU for the RAM machine: FC-like at 25 MHz (every other clock of
// the RAM's 50 MHz), one instruction a clock unless it stalls, with the
// T1 demo's working mix (fsim, the demo without its frame-wait loop):
//   30% of instructions are 32-bit (two parcels, else one)
//   17% load a word: the CPU waits for it
//   5.5% store a word: into a 4-entry write buffer; the CPU waits only
//        when it is full
//   6% are taken branches: the prefetch buffer empties and fetch starts
//        again at a random place
// Fetch: a 16-word prefetch buffer, refilled by bursts of 8 from the code
// region while it has room for one. Code and loads come from region T1
// (the pattern; every word checked); stores go to region T2, never the
// framebuffer, and a quarter of loads read back the last word stored
// (after the write buffer drains: the RAW check).
//
// Ports (rammux): w (single-word writes), r (single-word reads), f (bursts
// of 8). Counted, as pulses on clk: a CPU clock; an instruction done; a
// CPU clock stalled for fetch, a load, a store; an error.
module mockcpu #(
    parameter [17:0] T1 = 18'h20000,       // 32K words: the pattern (code and data)
    parameter [17:0] T2 = 18'h30000        // 32K words: stores
) (
    input  wire        clk,                 // 50 MHz
    input  wire        en,
    // write port
    output wire        w_req,
    output wire [17:0] w_addr,
    output wire [31:0] w_data,
    input  wire        w_ack,
    // read port
    output wire        r_req,
    output wire [17:0] r_addr,
    input  wire        r_ack,
    input  wire        r_valid,
    // fetch port
    output wire        f_req,
    output wire [17:0] f_addr,
    input  wire        f_ack,
    input  wire        f_valid,
    input  wire [31:0] rdata,
    // counts (pulses)
    output reg         c_cycle = 1'b0,
    output reg         c_inst = 1'b0,
    output reg         c_sfetch = 1'b0,
    output reg         c_sload = 1'b0,
    output reg         c_sstore = 1'b0,
    output reg         c_err_r = 1'b0,
    output reg         c_err_f = 1'b0
);
    function [31:0] pat(input [17:0] a);
        pat = {~a[13:0], a} ^ {a[5:0], 26'h2A5A5A5};
    endfunction
    function [31:0] lfsr(input [31:0] x);
        lfsr = {x[30:0], x[31] ^ x[21] ^ x[1] ^ x[0]};
    endfunction
    function [31:0] lfsr2(input [31:0] x);  // two steps (XST takes lfsr(lfsr()) for recursion)
        reg [31:0] y;
        begin
            y = {x[30:0], x[31] ^ x[21] ^ x[1] ^ x[0]};
            lfsr2 = {y[30:0], y[31] ^ y[21] ^ y[1] ^ y[0]};
        end
    endfunction

    reg         ce = 1'b0;                  // the CPU's clock enable: every other clock
    reg  [31:0] rnd = 32'h9E37_79B9, rnd2 = 32'h7F4A_7C15;

    // ---- write buffer (4)
    reg  [17:0] wb_a [0:3];
    reg  [1:0]  wb_w = 2'd0, wb_r = 2'd0;
    reg  [2:0]  wb_n = 3'd0;
    assign w_req  = wb_n != 3'd0;
    assign w_addr = wb_a[wb_r];
    assign w_data = pat(wb_a[wb_r]) ^ 32'hFFFF_0000;

    // ---- prefetch buffer (16 words: counted; the words checked as they arrive)
    reg  [4:0]  pf_n = 5'd0;                // words in the buffer
    reg  [4:0]  pf_out = 5'd0;              // words asked for, not yet back
    reg  [4:0]  pf_drop = 5'd0;             // of those, from before a branch
    reg  [14:0] pf_next = 15'd0;            // next word to ask for, in T1
    reg  [14:0] pf_chk = 15'd0;             // next word to come back
    reg         parc2 = 1'b1;               // the head word's two parcels are both there
    assign f_req  = en && ({1'b0, pf_n} + {1'b0, pf_out} <= 6'd8);
    assign f_addr = T1 + {3'd0, pf_next};
    wire [5:0]  parcels = {pf_n, 1'b0} - (parc2 ? 6'd0 : 6'd1);

    // ---- the instruction now
    localparam PF = 2'd0, PL = 2'd1;        // fetching, waiting for a load
    reg  [1:0]  ph = PF;
    reg         i32, ild, ist, ibr, iraw;
    reg  [17:0] ld_a = 18'd0, last_st = 18'd0;
    reg         st_any = 1'b0;
    reg         ld_wait = 1'b0;             // asked, the word not yet back
    reg         ld_got = 1'b0;              // the word is back
    assign r_req  = en && ph == PL && !ld_wait && !ld_got && !(iraw && wb_n != 3'd0);
    assign r_addr = ld_a;

    initial begin i32 = 1'b0; ild = 1'b0; ist = 1'b0; ibr = 1'b0; iraw = 1'b0; end

    // this CPU clock
    wire        cpu  = ce && en;
    wire        f_ok = ph == PF && parcels >= (i32 ? 6'd2 : 6'd1);
    wire        s_ok = !ist || wb_n != 3'd4 || w_ack;
    wire        done = cpu && ((f_ok && !ild && s_ok) || (ph == PL && ld_got));
    wire        pop  = cpu && f_ok && (ild || s_ok) && (i32 || !parc2);
    wire        push = done && ist;
    wire        flush = done && ibr;

    always @(posedge clk) begin
        ce <= ~ce;
        c_cycle <= cpu; c_inst <= done;
        c_sfetch <= cpu && ph == PF && !f_ok;
        c_sload  <= cpu && ph == PL && !ld_got;
        c_sstore <= cpu && f_ok && !ild && !s_ok;
        c_err_r <= 1'b0; c_err_f <= 1'b0;
        if (cpu) begin rnd <= lfsr2(rnd); rnd2 <= lfsr(rnd2); end

        // ---- write buffer
        if (w_ack) wb_r <= wb_r + 2'd1;
        if (push) begin
            wb_a[wb_w] <= T2 + {3'd0, rnd2[14:0]};
            last_st <= T2 + {3'd0, rnd2[14:0]};
            st_any <= 1'b1;
            wb_w <= wb_w + 2'd1;
        end
        wb_n <= wb_n + (push ? 3'd1 : 3'd0) - (w_ack ? 3'd1 : 3'd0);

        // ---- loads
        if (r_ack) ld_wait <= 1'b1;
        if (r_valid) begin
            ld_wait <= 1'b0; ld_got <= 1'b1;
            if (rdata != (iraw ? pat(ld_a) ^ 32'hFFFF_0000 : pat(ld_a))) c_err_r <= 1'b1;
        end

        // ---- fetch
        if (f_valid && pf_drop == 5'd0) begin
            if (rdata != pat(T1 + {3'd0, pf_chk})) c_err_f <= 1'b1;
            pf_chk <= pf_chk + 15'd1;
        end
        if (f_valid && pf_drop != 5'd0) pf_drop <= pf_drop - 5'd1;
        pf_out <= pf_out + (f_ack ? 5'd8 : 5'd0) - (f_valid ? 5'd1 : 5'd0);
        pf_n <= pf_n + ((f_valid && pf_drop == 5'd0) ? 5'd1 : 5'd0) - (pop ? 5'd1 : 5'd0);
        if (f_ack) pf_next <= pf_next + 15'd8;
        if (cpu && f_ok && (ild || s_ok)) parc2 <= i32 ? parc2 : !parc2;

        // ---- the instruction's progress
        if (cpu && f_ok && ild) begin ph <= PL; ld_got <= 1'b0; end
        if (done) begin
            ph <= PF; ld_got <= 1'b0;
            // the next instruction
            i32  <= rnd[7:0] < 8'd77;                               // 30%
            ild  <= rnd[15:8] < 8'd44;                              // 17%
            ist  <= rnd[15:8] >= 8'd44 && rnd[15:8] < 8'd58;        // 5.5%
            ibr  <= rnd[23:16] < 8'd15;                             // 6%
            iraw <= (st_any || push) && rnd[25:24] == 2'd0;
            ld_a <= ((st_any || push) && rnd[25:24] == 2'd0)
                    ? (push ? T2 + {3'd0, rnd2[14:0]} : last_st)
                    : T1 + {3'd0, rnd2[31:17]};
        end
        if (flush) begin
            // a taken branch: empty the buffer, drop what is still coming, fetch elsewhere
            pf_n <= 5'd0;
            pf_drop <= pf_out + (f_ack ? 5'd8 : 5'd0) - (f_valid ? 5'd1 : 5'd0);
            pf_next <= {rnd[30:19], 3'b000};
            pf_chk  <= {rnd[30:19], 3'b000};
            parc2 <= 1'b1;
        end
    end
endmodule
