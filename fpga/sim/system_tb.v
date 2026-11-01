`timescale 1ns/1ps
// The whole system with a program in SRAM: T1, memsys, I/O, video and the
// SRAM model. The program's console output (CON_OUT) is printed, and EXIT
// ends the run with its code, as fsim does.
//
//   vvp system_tb.vvp +image=prog.hex [+max=CYCLES] [+shot=FILE]
//
// prog.hex comes from fpga/tools/bin2hex.py (32-bit words, @ addresses).
// +shot writes the first whole video frame after the program ends (or
// after +max 50 MHz cycles) as one '0'-'7' character per pixel, for
// fpga/tools/cmpshot.py to compare with fsim --screenshot.
module system_tb;
    reg clk = 0;
    always #10 clk = ~clk;

    wire [17:0] sram_a;
    wire [31:0] sram_d;
    wire        sram_oe_n, sram_we_n;
    wire [1:0]  sram_ce_n;
    wire [3:0]  sram_be_n;
    wire [7:0]  led;
    wire [3:0]  an;
    wire [7:0]  seg;
    wire        vr, vg, vb, hs, vs;

    system dut (
        .clk(clk), .btn(4'b0000), .sw(8'h00), .led(led), .an(an), .seg(seg),
        .vga_r(vr), .vga_g(vg), .vga_b(vb), .vga_hs(hs), .vga_vs(vs),
        .sram_a(sram_a), .sram_d(sram_d), .sram_oe_n(sram_oe_n), .sram_we_n(sram_we_n),
        .sram_ce_n(sram_ce_n), .sram_be_n(sram_be_n)
    );
    sram_model sram (.a(sram_a), .d(sram_d), .oe_n(sram_oe_n), .we_n(sram_we_n),
                     .ce_n(sram_ce_n), .be_n(sram_be_n));

    integer max_cycles = 200000000;
    integer cycles = 0;
    reg [1023:0] shot;
    reg want_shot = 0, ending = 0;
    integer code = 0;

    // period counts by owner
    integer slots_cpu = 0, slots_dma = 0, slots_ldr = 0, dma_words = 0;
    always @(posedge dut.clk50) begin
        if (dut.mem.ph == 0) begin
            if (dut.mem.slot == 0) slots_cpu = slots_cpu + 1;
            if (dut.mem.slot == 1) slots_dma = slots_dma + 1;
            if (dut.mem.slot == 2) slots_ldr = slots_ldr + 1;
        end
        if (dut.mem.dma_valid) dma_words = dma_words + 1;
    end


    initial begin
        if ($value$plusargs("max=%d", max_cycles)) ;
        want_shot = $value$plusargs("shot=%s", shot);
        #2000 dut.ldr.run = 1'b1;
    end

    always @(posedge dut.clk50) begin
        cycles = cycles + 1;
        if (dut.io.con_we) begin
            $write("%c", dut.io.wvalue[7:0]);
            $fflush;
        end
        if (!ending && dut.io.exit_we) begin
            code = dut.io.wvalue;
            finish_run("exit");
        end
        if (!ending && dut.halted) begin
            code = 256 + dut.halt_code;
            finish_run("halt");
        end
        if (!ending && cycles >= max_cycles) finish_run("max cycles");
    end

    task finish_run(input [8*16-1:0] why);
        integer k;
        reg [31:0] dumpw;
        begin
            if ($value$plusargs("dumpw=%h", dumpw))
                for (k = 0; k < 5; k = k + 1)
                    $display("[system_tb] word %06x = %08x", (dumpw + 4 * k), sram.mem[(dumpw >> 2) + k]);
            ending = 1;
            $display("\n[system_tb] %0s: code %0d, T1 cycles %0d, instructions %0d, %0d x 50 MHz cycles",
                     why, code, dut.cycles, dut.insts, cycles);
            $display("[system_tb] periods: cpu %0d, dma %0d, loader %0d; dma words %0d",
                     slots_cpu, slots_dma, slots_ldr, dma_words);
            if (!want_shot) $finish;
        end
    endtask

    // +trace=N: T1's first N bus accesses
    integer trace = 0;
    initial if ($value$plusargs("trace=%d", trace)) ;
    always @(posedge dut.cpu_clk)
        if (trace > 0 && dut.run && (dut.cpu_rd || dut.cpu_wr)) begin
            $display("[t1] %06x %s %02x", dut.cpu_addr, dut.cpu_wr ? "W" : "R",
                     dut.cpu_wr ? dut.cpu_dout : dut.cpu_din);
            trace = trace - 1;
        end

    // ---- frame capture: rgb comes out four pixel clocks after the counters
    reg [10:0] h1 = 0, h2 = 0, h3 = 0, h4 = 0;
    reg [9:0]  v1 = 0, v2 = 0, v3 = 0, v4 = 0;
    integer f, state = 0, warm = 0;
    always @(posedge dut.pclk) begin
        if (ending && want_shot && warm >= 4) begin
            if (state == 0 && h4 == 0 && v4 == 0) begin
                f = $fopen(shot, "w");
                state = 1;
            end
            if (state == 1 && h4 < 1280 && v4 < 720) begin
                $fwrite(f, "%0d", {vr, vg, vb});
                if (h4 == 1279) $fwrite(f, "\n");
                if (h4 == 1279 && v4 == 719) begin
                    $fclose(f);
                    $display("[system_tb] frame written to %0s", shot);
                    $finish;
                end
            end
        end
        warm = warm + 1;
        h1 <= dut.vid.hc; h2 <= h1; h3 <= h2; h4 <= h3;
        v1 <= dut.vid.vc; v2 <= v1; v3 <= v2; v4 <= v3;
    end
endmodule
