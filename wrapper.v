//Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
//Copyright 2022-2024 Advanced Micro Devices, Inc. All Rights Reserved.
//--------------------------------------------------------------------------------
//Tool Version: Vivado v.2024.2 (win64) Build 5239630 Fri Nov 08 22:35:27 MST 2024
//Date        : Fri Feb 20 17:42:32 2026
//Host        : Shivani running 64-bit major release  (build 9200)
//Command     : generate_target fir_filter_block_wrapper.bd
//Design      : fir_filter_block_wrapper
//Purpose     : IP block netlist
//--------------------------------------------------------------------------------
`timescale 1 ps / 1 ps

module fir_filter_block_wrapper
   (clk_in1_0,
    reset_rtl_0,
    x_in,
    y_out);
  input clk_in1_0;
  input reset_rtl_0;
  input [15:0]x_in;
  output [15:0]y_out;

  wire clk_in1_0;
  wire reset_rtl_0;
  wire [15:0]x_in;
  wire [15:0]y_out;

  fir_filter_block fir_filter_block_i
       (.clk_in1_0(clk_in1_0),
        .reset_rtl_0(reset_rtl_0),
        .x_in(x_in),
        .y_out(y_out));
endmodule
