`timescale 1ns/1ps
// hdmiaudio: run a few frames and write what dvi_tx puts on its three data
// channels, one 10-bit word per pixel clock, for tools/hdmi_audio_check.py.
// +out=FILE (default build/hdmiaudio.words), +frames=N (default 3).
// Each line: ch0 ch1 ch2 in hex. The first frame is skipped by the checker.
module hdmiaudio_tb;
    reg clk = 0;
    always #18.518 clk = ~clk;              // 27 MHz
    wire [5:0] led;
    hdmiaudio dut (.clk(clk), .led(led), .tmds_p(), .tmds_n());

    integer f, frames, n = 0;
    reg [1023:0] file;
    initial begin
        if (!$value$plusargs("out=%s", file)) file = "hdmiaudio.words";
        if (!$value$plusargs("frames=%d", frames)) frames = 3;
        f = $fopen(file, "w");
        // 1,237,500 pixel clocks a frame
        #(13.468 * 1237500.0 * frames + 30000);
        $fclose(f);
        $display("wrote %0d words, led=%b", n, led);
        $finish;
    end
    always @(posedge dut.pclk) begin
        $fwrite(f, "%h %h %h\n", dut.tx.words[0], dut.tx.words[1], dut.tx.words[2]);
        n = n + 1;
    end
endmodule
