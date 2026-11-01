`timescale 1ns/1ps
// system: load a program over the serial line (+bytes=FILE: one hex byte a
// line, loaded at +base, default 0x1000), run it, and print what it sends;
// 0x04 and a code byte end the run, as tang/tools/run.py expects.
// +shot=FILE +frame=N: write video frame N (counted from the start) as one
// '0'-'7' character per pixel (each channel's top bit), for
// fpga/tools/cmpshot.py, and stop.
module system_tb;
    reg clk = 0;
    always #74 clk = ~clk;                  // 6.75 MHz
    reg rx = 1'b1;
    wire tx;
    wire [5:0] led;
    system dut (.clk(clk), .s1(1'b0), .s2(1'b0), .uart_rx(rx), .uart_tx(tx), .led(led), .tmds_p(), .tmds_n());

    localparam BIT = 4 * 148;               // the simulation baud rate
    task send(input [7:0] b);
        integer i;
        begin
            rx = 1'b0; #BIT;
            for (i = 0; i < 8; i = i + 1) begin rx = b[i]; #BIT; end
            rx = 1'b1; #BIT;
        end
    endtask

    reg [7:0] prog [0:65535];
    integer n, k, base;
    reg [1023:0] file;
    initial begin
        for (k = 0; k < 65536; k = k + 1) prog[k] = 8'hxx;
        if (!$value$plusargs("bytes=%s", file)) begin $display("need +bytes=FILE"); $finish; end
        if (!$value$plusargs("base=%h", base)) base = 32'h1000;
        $readmemh(file, prog);
        n = 0; while (n < 65536 && prog[n] !== 8'hxx) n = n + 1;
        #2000;
        send("L"); send(base >> 16); send(base >> 8); send(base);
        send(n >> 16); send(n >> 8); send(n);
        for (k = 0; k < n; k = k + 1) send(prog[k]);
        send("G"); send(base >> 16); send(base >> 8); send(base);
    end

    // receive what T1 sends
    integer i, got_eot = 0;
    reg [7:0] c;
    always begin
        @(negedge tx);
        #(BIT / 2);
        for (i = 0; i < 8; i = i + 1) begin #BIT; c[i] = tx; end
        #BIT;
        if (got_eot) begin
            $display("\n[system_tb] exit code %0d, T1 cycles %0d", c, dut.cycles);
            $finish;
        end else if (c == 8'h04) got_eot = 1;
        else begin $write("%c", c); $fflush; end
    end

    // ---- frame capture, from the video's 24-bit output and data enable
    reg [1023:0] shot;
    integer want = -1, fno = 0, fx = 0, fy = 0, ff = 0;
    reg de_d = 1'b0;
    initial if ($value$plusargs("shot=%s", shot)) begin
        if (!$value$plusargs("frame=%d", want)) want = 3;
    end
    always @(posedge dut.pclk) begin
        de_d <= dut.de;
        if (dut.de) begin
            if (fno == want) begin
                if (fx == 0 && fy == 0) ff = $fopen(shot, "w");
                $fwrite(ff, "%0d", {dut.rgb24[23], dut.rgb24[15], dut.rgb24[7]});
                if (fx == 1279) $fwrite(ff, "\n");
                if (fx == 1279 && fy == 719) begin
                    $fclose(ff);
                    $display("[system_tb] frame %0d written to %0s", want, shot);
                    $finish;
                end
            end
            fx = (fx == 1279) ? 0 : fx + 1;
            if (fx == 0) begin
                fy = (fy == 719) ? 0 : fy + 1;
                if (fy == 0) fno = fno + 1;
            end
        end
    end

    initial begin #3000000000; $display("\n[system_tb] timeout"); $finish; end
endmodule
