module flash_avalon (
    input            clk,
    input            reset_n,
    input      [2:0] avs_address,
    input            avs_read,
    output reg [31:0] avs_readdata,
    input            avs_write,
    input      [31:0] avs_writedata,
    output     [21:0] FL_ADDR,
    inout      [7:0]  FL_DQ,
    output            FL_WE_N,
    output            FL_CE_N,
    output            FL_OE_N,
    output            FL_RST_N
);

reg [21:0] addr_reg;
reg [7:0]  wdata_reg;
reg [2:0]  cmd_reg;
reg        start_reg;
reg [4:0]  start_count;
reg        op_active;
reg        seen_not_ready;
wire [7:0] rdata_wire;
wire       ready_wire;

flash_controller_de2 u_flash (
    .oDATA   (rdata_wire),
    .iDATA   (wdata_reg),
    .iADDR   (addr_reg),
    .iCMD    (cmd_reg),
    .oReady  (ready_wire),
    .iStart  (start_reg),
    .iCLK    (clk),
    .iRST_n  (reset_n),
    .FL_DQ   (FL_DQ),
    .FL_ADDR (FL_ADDR),
    .FL_WE_n (FL_WE_N),
    .FL_CE_n (FL_CE_N),
    .FL_OE_n (FL_OE_N),
    .FL_RST_n(FL_RST_N)
);

always @(posedge clk or negedge reset_n) begin
    if (!reset_n) begin
        addr_reg <= 22'h0;
        wdata_reg <= 8'h0;
        cmd_reg <= 3'h0;
        start_reg <= 1'b0;
        start_count <= 5'd0;
        op_active <= 1'b0;
        seen_not_ready <= 1'b0;
    end else begin
        if (avs_write) begin
            case (avs_address)
                3'd0: addr_reg <= avs_writedata[21:0];
                3'd1: wdata_reg <= avs_writedata[7:0];
                3'd2: cmd_reg <= avs_writedata[2:0];
                3'd3: begin
                    if (avs_writedata[0]) begin
                        start_reg <= 1'b1;
                        start_count <= 5'd16;
                        op_active <= 1'b1;
                        seen_not_ready <= 1'b0;
                    end
                end
                default: begin end
            endcase
        end

        if (start_count != 0) begin
            start_count <= start_count - 1'b1;
            if (start_count == 1)
                start_reg <= 1'b0;
        end

        if (op_active) begin
            if (!ready_wire)
                seen_not_ready <= 1'b1;

            if (seen_not_ready && ready_wire)
                op_active <= 1'b0;
        end
    end
end

always @(*) begin
    avs_readdata = 32'h0;
    if (avs_read) begin
        case (avs_address)
            3'd0: avs_readdata = {10'h0, addr_reg};
            3'd1: avs_readdata = {24'h0, wdata_reg};
            3'd2: avs_readdata = {29'h0, cmd_reg};
            3'd3: avs_readdata = {31'h0, (~op_active)};
            3'd4: avs_readdata = {24'h0, rdata_wire};
            default: avs_readdata = 32'h0;
        endcase
    end
end

endmodule
