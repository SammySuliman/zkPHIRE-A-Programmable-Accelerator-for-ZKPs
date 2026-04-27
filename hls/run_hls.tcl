# zkPHIRE SumCheck HLS Build Script
#
# Usage:
#   vitis_hls -f run_hls.tcl
#
# Targets the PYNQ-Z2 (xc7z020clg400-1) at 100 MHz.
# The build sequence:
#   1. C-simulation (verifies bit-exact match with golden model)
#   2. C-synthesis (generates RTL)
#   3. Co-simulation (optional RTL/SystemC co-sim)
#

# ------------------------------------------------------------------
# Project setup
# ------------------------------------------------------------------
open_project -reset zkphire_sumcheck

set_top sumcheck_top

# Design sources
add_files -cflags "-I." src/sumcheck_top.cpp

# Testbench
add_files -tb testbench/tb_sumcheck.cpp

# ------------------------------------------------------------------
# Solution configuration
# ------------------------------------------------------------------
open_solution -reset "solution1"

# Target: PYNQ-Z2 (xc7z020clg400-1) per SPEC Section 12
set_part {xc7z020clg400-1}

# Clock: 100 MHz (10 ns period) per SPEC Section 12
create_clock -period 10 -name default

# ------------------------------------------------------------------
# C-Simulation
# ------------------------------------------------------------------
puts "--- C-Simulation ---"
if {[catch {csim_design} res]} {
    puts "FAIL: C-Simulation failed."
    puts $res
    exit 1
}
puts "PASS: C-Simulation passed."

# ------------------------------------------------------------------
# C-Synthesis
# ------------------------------------------------------------------
puts "--- C-Synthesis ---"
if {[catch {csynth_design} res]} {
    puts "FAIL: C-Synthesis failed."
    puts $res
    exit 1
}
puts "PASS: C-Synthesis passed."

# ------------------------------------------------------------------
# Co-Simulation (optional — uncomment to enable)
# ------------------------------------------------------------------
# puts "--- Co-Simulation ---"
# if {[catch {cosim_design} res]} {
#     puts "WARNING: Co-Simulation failed."
#     puts $res
# } else {
#     puts "PASS: Co-Simulation passed."
# }

# ------------------------------------------------------------------
# Export IP
# ------------------------------------------------------------------
puts "--- Exporting IP ---"
if {[catch {export_design -format ip_catalog} res]} {
    puts "WARNING: IP export failed."
    puts $res
} else {
    puts "PASS: IP exported."
}

puts ""
puts "===== zkPHIRE SumCheck HLS Build Complete ====="
exit 0
