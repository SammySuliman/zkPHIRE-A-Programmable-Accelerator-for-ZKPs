set script_dir [file dirname [file normalize [info script]]]
set hls_dir [file normalize "$script_dir/.."]
set repo_dir [file normalize "$hls_dir/.."]
set project_dir "$hls_dir/sumcheck_round_prj"

open_project -reset $project_dir
set_top sumcheck_round_axis

add_files "$hls_dir/src/sumcheck_top.cpp" -cflags "-I$hls_dir/include"
add_files -tb "$hls_dir/tb/sumcheck_tb.cpp" -cflags "-I$hls_dir/include"

open_solution -reset "solution1"
set_part {xc7z020clg400-1}
create_clock -period 10 -name default

csim_design

if {[info exists ::env(RUN_CSYNTH)] && $::env(RUN_CSYNTH) == "1"} {
    csynth_design
    export_design -format ip_catalog
}

close_project
