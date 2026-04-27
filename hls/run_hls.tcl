# zkPHIRE SumCheck HLS Build Script
#
# Usage:
#   vitis_hls -f run_hls.tcl
#
# Targets the PYNQ-Z2 (xc7z020clg400-1) at 100 MHz.
# The build sequence:
#   1. C-simulation (verifies bit-exact match with golden model)
#   2. C-synthesis (generates RTL)
#   3. Optional co-simulation
#   4. IP export
#
# Two top functions are exposed:
#   sumcheck_round_array  — C-sim friendly, BRAM interface   (default for csim)
#   sumcheck_round_axi    — m_axi interface, board-ready

# ------------------------------------------------------------------
# Project setup
# ------------------------------------------------------------------
open_project -reset zkphire_sumcheck

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
# C-Simulation (using array API for fast verification)
# ------------------------------------------------------------------
set_top sumcheck_round_array

puts "--- C-Simulation ---"
if {[catch {csim_design} res]} {
    puts "FAIL: C-Simulation failed."
    puts $res
    exit 1
}
puts "PASS: C-Simulation passed."

# ------------------------------------------------------------------
# C-Synthesis (synthesize both top functions)
# ------------------------------------------------------------------
puts "--- C-Synthesis (array API) ---"
if {[catch {csynth_design} res]} {
    puts "FAIL: C-Synthesis failed for sumcheck_round_array."
    puts $res
    exit 1
}

# Synthesize the AXI version too
set_top sumcheck_round_axi
puts "--- C-Synthesis (AXI API) ---"
if {[catch {csynth_design} res]} {
    puts "WARNING: C-Synthesis failed for sumcheck_round_axi."
    puts $res
} else {
    puts "PASS: Both APIs synthesized."
}

# ------------------------------------------------------------------
# Co-Simulation (optional — uncomment to enable)
# ------------------------------------------------------------------
# set_top sumcheck_round_array
# puts "--- Co-Simulation ---"
# if {[catch {cosim_design} res]} {
#     puts "WARNING: Co-Simulation failed."
#     puts $res
# }

# ------------------------------------------------------------------
# Export IP (use the AXI version for board integration)
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
