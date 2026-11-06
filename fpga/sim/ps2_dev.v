`timescale 1ns/1ps
// A PS/2 keyboard or mouse for the system test bench (fpga/sim/progs/ps2test.c):
// open drain on clk and dat (the bench pulls them up). It answers reset (FF:
// FA AA, and 00 from a mouse), identify (F2: FA AB 83, or FA 00), and FA to
// anything else; after its NCMD-th command and IDLE ns it sends its script:
// the keyboard types "Hi!\n" (set 2), the mouse moves (5,-3) with the left
// button and (10,2) with none. A clock of 1/(2 HALF) (25 kHz by default:
// faster than a real device, for a short simulation).
module ps2_dev #(
    parameter MOUSE = 0,
    parameter HALF = 20000,
    parameter IDLE = 40000000
) (
    inout wire clk,
    inout wire dat
);
    reg c_lo = 1'b0, d_lo = 1'b0;
    assign clk = c_lo ? 1'b0 : 1'bz;
    assign dat = d_lo ? 1'b0 : 1'bz;

    reg [7:0] q [0:63];
    integer qh = 0, qt = 0, ncmd = 0, scripted = 0, i;
    time last = 0;
    reg [7:0] b;
    reg [10:0] f;
    task put(input [7:0] v); begin q[qt] = v; qt = qt + 1; end endtask

    // the host's byte, after its request to send (clock released, data low)
    task recv; begin
        for (i = 0; i < 10; i = i + 1) begin
            #(HALF / 2) c_lo = 1'b1;
            #HALF c_lo = 1'b0;
            f[i] = (dat !== 1'b0);
            #(HALF / 2);
        end
        #(HALF / 2) d_lo = 1'b1; c_lo = 1'b1;     // the acknowledge
        #HALF c_lo = 1'b0;
        #(HALF / 2) d_lo = 1'b0;
        b = f[7:0];
        if (^f[8:0] !== 1'b1) $display("[ps2_dev] parity error on %h", b);
`ifdef PS2_DEBUG
        $display("[ps2_dev %0d] %0t: got %h (frame %b)", MOUSE, $time, b, f[9:0]);
`endif
    end endtask

    // a byte to the host; false if the host held the clock (inhibit) first
    task send(input [7:0] v); begin
        f = {1'b1, ~^v, v, 1'b0};
        for (i = 0; i < 11; i = i + 1) begin
            d_lo = !f[i];
            #(HALF / 2) c_lo = 1'b1;
            #HALF c_lo = 1'b0;
            #(HALF / 2);
        end
        d_lo = 1'b0;
    end endtask

    integer high = 0;
    always begin
        #1000;
        if (clk === 1'b1 && dat === 1'b0 && !d_lo) begin
`ifdef PS2_DEBUG
            $display("[ps2_dev %0d] %0t: request to send", MOUSE, $time);
`endif
            #5000 recv;
            ncmd = ncmd + 1;
            last = $time;
            case (b)
                8'hFF: begin qh = 0; qt = 0; put(8'hFA); put(8'hAA); if (MOUSE) put(8'h00); end
                8'hF2: begin put(8'hFA); if (MOUSE) put(8'h00); else begin put(8'hAB); put(8'h83); end end
                default: put(8'hFA);
            endcase
            high = 0;
        end else if (clk === 1'b1) begin
            high = high + 1;
            if (qh != qt && high >= 50) begin
                send(q[qh]);
                qh = qh + 1;
                high = 0;
            end
        end else
            high = 0;
        if (!scripted && ncmd >= (MOUSE ? 3 : 2) && qh == qt && $time - last > IDLE) begin
            scripted = 1;
            if (MOUSE) begin
                put(8'h29); put(8'h05); put(8'hFD);
                put(8'h08); put(8'h0A); put(8'h02);
            end else begin
                put(8'h12); put(8'h33); put(8'hF0); put(8'h33); put(8'hF0); put(8'h12);   // H
                put(8'h43); put(8'hF0); put(8'h43);                                       // i
                put(8'h12); put(8'h16); put(8'hF0); put(8'h16); put(8'hF0); put(8'h12);   // !
                put(8'h5A); put(8'hF0); put(8'h5A);                                       // Enter
            end
        end
    end
endmodule
