# (C) Copyright 2020 - 2022 Xilinx, Inc.
# SPDX-License-Identifier: Apache-2.0
#
# Updated for custom made RS-485 PMOD module for Eagle-Eye-AI Application
#
# Link to module: 
#    https://github.com/s59mz/kicad-pmod_rs485

#MIPI
set_property DIFF_TERM_ADV TERM_100 [get_ports {mipi_phy_if_clk_p}]
set_property DIFF_TERM_ADV TERM_100 [get_ports {mipi_phy_if_clk_n}]
set_property DIFF_TERM_ADV TERM_100 [get_ports {mipi_phy_if_data_p[*]}]
set_property DIFF_TERM_ADV TERM_100 [get_ports {mipi_phy_if_data_n[*]}]


#GPIO
#ISP AP1302_RST_B HDA02
set_property PACKAGE_PIN J11 [get_ports {ap1302_rst_b}]
set_property IOSTANDARD LVCMOS33 [get_ports {ap1302_rst_b}]
set_property SLEW SLOW [get_ports {ap1302_rst_b}]
set_property DRIVE 4 [get_ports {ap1302_rst_b}]

#ISP AP1302_STANDBY HDA03
set_property PACKAGE_PIN J10 [get_ports {ap1302_standby}]
set_property IOSTANDARD LVCMOS33 [get_ports {ap1302_standby}]
set_property SLEW SLOW [get_ports {ap1302_standby}]
set_property DRIVE 4 [get_ports {ap1302_standby}]

#Fan Speed Enable
set_property PACKAGE_PIN A12 [get_ports {fan_en_b}]
set_property IOSTANDARD LVCMOS33 [get_ports {fan_en_b}]
set_property SLEW SLOW [get_ports {fan_en_b}]
set_property DRIVE 4 [get_ports {fan_en_b}]


#I2C signals --> I2C switch 0--> ISP AP1302 + Sensor AR1335
set_property PACKAGE_PIN G11 [get_ports iic_scl_io]
set_property PACKAGE_PIN F10 [get_ports iic_sda_io]
set_property IOSTANDARD LVCMOS33 [get_ports iic_*]
set_property SLEW SLOW [get_ports iic_*]
set_property DRIVE 4 [get_ports iic_*]

#
## PMOD-1 RS-485 module pins
set_property PACKAGE_PIN H12 [get_ports pmod1_uart_rxn];     # PMOD-1 pin-1 Rx-disable
set_property PACKAGE_PIN B10 [get_ports pmod1_led_tri_o[0]]; # PMOD-1 pin-2 LED-1
set_property PACKAGE_PIN E10 [get_ports pmod1_uart_txd];     # PMOD-1 pin-3 Tx
set_property PACKAGE_PIN E12 [get_ports pmod1_led_tri_o[1]]; # PMOD-1 pin-4 LED-2
set_property PACKAGE_PIN D10 [get_ports pmod1_uart_rxd];     # PMOD-1 pin-5 Rx
set_property PACKAGE_PIN D11 [get_ports pmod1_btn_tri_i[0]]; # PMOD-1 pin-6 Btn-1
set_property PACKAGE_PIN C11 [get_ports pmod1_uart_txen];    # PMOD-1 pin-7 Tx-enable
set_property PACKAGE_PIN B11 [get_ports pmod1_btn_tri_i[1]]; # PMOD-1 pin-8 Btn-2

set_property IOSTANDARD LVCMOS33 [get_ports pmod1_*];
set_property SLEW SLOW [get_ports pmod1_*]
set_property DRIVE 4 [get_ports pmod1_*]

set_property BITSTREAM.CONFIG.OVERTEMPSHUTDOWN ENABLE [current_design]
