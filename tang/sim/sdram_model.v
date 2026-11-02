`timescale 1ns/1ps
// Behavioral 2M x 32 SDR SDRAM (4 banks x 2048 rows x 256 columns) for the
// Tang testbench: mode register (CAS latency, burst length 1 only), ACT,
// READ, WRITE with DQM, PRE (one bank or all), REF. Read data is driven
// tAC after the CAS-latency edge and held tOH after the next. Checks tRCD,
// tRP, reads and writes to closed banks and ACT to an open bank.
module sdram_model #(
    parameter real T_AC = 5.4,
    parameter real T_OH = 2.5
) (
    input  wire        clk,
    input  wire        cke,
    input  wire        cs_n, ras_n, cas_n, we_n,
    input  wire [10:0] a,
    input  wire [1:0]  ba,
    input  wire [3:0]  dqm,
    inout  wire [31:0] dq
);
    reg [31:0] mem [0:2097151];
    reg [10:0] row [0:3];
    reg [3:0]  open = 4'b0;
    integer    t_act [0:3], t_pre [0:3];
    integer    cyc = 0, cl = 2, errors = 0, i, j;
    initial for (i = 0; i < 4; i = i + 1) begin t_act[i] = -100; t_pre[i] = -100; row[i] = 0; end
    initial for (j = 0; j < 2097152; j = j + 1) mem[j] = 32'd0;   // zeroed, as fsim's memory

    reg [31:0] dq_out = 32'd0;
    reg        dq_en = 1'b0;
    assign dq = dq_en ? dq_out : 32'bz;

    // read pipeline: valid and word per cycle of latency
    reg        rv [0:3];
    reg [31:0] rw [0:3];
    initial for (i = 0; i < 4; i = i + 1) begin rv[i] = 1'b0; rw[i] = 0; end

    wire [3:0] cmd = {cs_n, ras_n, cas_n, we_n};
    wire [20:0] waddr = {row[ba], ba, a[7:0]};

    task err(input [8*48-1:0] what);
        begin
            errors = errors + 1;
            if (errors <= 20) $display("[sdram] %0t: %0s (bank %0d)", $time, what, ba);
        end
    endtask

    always @(posedge clk) begin
        cyc = cyc + 1;
        // the data leaving the pipeline: drive tAC after this edge, hold tOH after the next
        for (i = 3; i > 0; i = i - 1) begin rv[i] = rv[i - 1]; rw[i] = rw[i - 1]; end
        rv[0] = 1'b0;
        if (rv[cl]) begin
            dq_out <= #(T_AC) rw[cl];
            dq_en  <= #(T_AC) 1'b1;
        end else
            dq_en  <= #(T_OH) 1'b0;

        if (cke) case (cmd)
            4'b0011: begin                  // ACT
                if (open[ba]) err("ACT to an open bank");
                if (cyc - t_pre[ba] < 2) err("tRP");
                open[ba] = 1'b1; row[ba] = a; t_act[ba] = cyc;
            end
            4'b0101, 4'b0100: begin         // READ, WRITE
                if (!open[ba]) err("access to a closed bank");
                if (cyc - t_act[ba] < 2) err("tRCD");
                if (a[10]) err("auto-precharge not modelled");
                if (we_n) begin
                    rv[0] = 1'b1;
                    rw[0] = mem[waddr];
                end else begin
                    if (!dqm[0]) mem[waddr][7:0]   = dq[7:0];
                    if (!dqm[1]) mem[waddr][15:8]  = dq[15:8];
                    if (!dqm[2]) mem[waddr][23:16] = dq[23:16];
                    if (!dqm[3]) mem[waddr][31:24] = dq[31:24];
                end
            end
            4'b0010: begin                  // PRE
                if (a[10]) begin
                    for (i = 0; i < 4; i = i + 1) if (open[i]) begin
                        if (cyc - t_act[i] < 3) err("tRAS");
                        t_pre[i] = cyc;
                    end
                    open = 4'b0;
                end else begin
                    if (open[ba] && cyc - t_act[ba] < 3) err("tRAS");
                    open[ba] = 1'b0; t_pre[ba] = cyc;
                end
            end
            4'b0001: if (open != 0) err("REF with a bank open");
            4'b0000: begin
                cl = a[6:4];
                if (a[2:0] != 3'b000) err("burst length other than 1");
            end
            default: ;
        endcase
    end
endmodule
