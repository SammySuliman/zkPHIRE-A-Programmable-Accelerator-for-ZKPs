# zkPHIRE SumCheck HLS Build Script
#
# Usage:
#   vitis_hls -f run_hls.tcl
#
# Targets the PYNQ-Z2 (xc7z020clg400-1) at 100 MHz.

# Compute absolute path to this script's directory for include paths
set script_dir [file dirname [file normalize [info script]]]

# ------------------------------------------------------------------
# Project setup
# ------------------------------------------------------------------
open_project -reset zkphire_sumcheck
set_top sumcheck_round_array

# Design sources (include path relative to hls/ root)
add_files -cflags "-I$script_dir" src/sumcheck_top.cpp

# Testbench
add_files -tb -cflags "-I$script_dir" testbench/tb_sumcheck.cpp

# ------------------------------------------------------------------
# Solution configuration
# ------------------------------------------------------------------
open_solution -reset "solution1"
set_part {xc7z020clg400-1}
create_clock -period 10 -name default

# ------------------------------------------------------------------
# C-Simulation (array API for fast verification)
# ------------------------------------------------------------------
puts "--- C-Simulation ---"
if {[catch {csim_design} res]} {
    puts "FAIL: C-Simulation failed."
    puts $res
    exit 1
}
puts "PASS: C-Simulation passed."

# ------------------------------------------------------------------
# C-Synthesis (both APIs)
# ------------------------------------------------------------------
puts "--- C-Synthesis (array API) ---"
if {[catch {csynth_design} res]} {
    puts "FAIL: C-Synthesis failed for sumcheck_round_array."
    puts $res
    exit 1
}

set_top sumcheck_round_axi
puts "--- C-Synthesis (AXI API) ---"
if {[catch {csynth_design} res]} {
    puts "WARNING: C-Synthesis failed for sumcheck_round_axi."
    puts $res
} else {
    puts "PASS: Both APIs synthesized."
}

# ------------------------------------------------------------------
# Export IP (AXI version for board integration)
# ------------------------------------------------------------------
set_top sumcheck_round_axi
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
