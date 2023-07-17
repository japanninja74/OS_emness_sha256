set design_name 	EmnessSha256

set script_dir    ./scripts
set vivado_dir 	./vivado
set out_dir       ./out

set proj_dir 		$vivado_dir/$design_name
set proj_src      $proj_dir/EmnessSha256.srcs
set repo_dir    	$vivado_dir/ip_repo

set gv_sha256_src	[glob -directory ./src/ip/hdl *]
set constr_file   ./src/soc/constrs.xdc

################################################################
# Check if script is running in correct Vivado version.
################################################################
set scripts_vivado_version 2023.1
set current_vivado_version [version -short]

if { [string first $scripts_vivado_version $current_vivado_version] == -1 } {
   puts ""
   catch {common::send_gid_msg -ssname BD::TCL -id 2041 -severity "ERROR" "This script was generated using Vivado <$scripts_vivado_version> and is being run in <$current_vivado_version> of Vivado. Please run the script in Vivado <$scripts_vivado_version> then open the design in Vivado <$current_vivado_version>. Upgrade the design by running \"Tools => Report => Report IP Status...\", then run write_bd_tcl to create an updated script."}

   return 1
}

################################################################
# Create project.
################################################################
create_project $design_name $proj_dir -part xc7z020clg400-1
set_property board_part tul.com.tw:pynq-z2:part0:1.0 [current_project]
set_property target_language VHDL [current_project]

################################################################
# Pin planning.
################################################################
add_files -fileset constrs_1 -norecurse $constr_file

################################################################
# Create cryptocore.
################################################################
create_peripheral PoliTo OperatingSystems04JEZOQ gv_sha256 1.0 -dir $repo_dir
add_peripheral_interface S00_AXI -interface_mode slave -axi_type lite [ipx::find_open_core PoliTo:OperatingSystems04JEZOQ:gv_sha256:1.0]
generate_peripheral [ipx::find_open_core PoliTo:OperatingSystems04JEZOQ:gv_sha256:1.0]
write_peripheral [ipx::find_open_core PoliTo:OperatingSystems04JEZOQ:gv_sha256:1.0]
set_property  ip_repo_paths  $repo_dir/gv_sha256_1_0 [current_project]
update_ip_catalog -rebuild

ipx::edit_ip_in_project -name gv_sha256_v1_0_project -directory $proj_dir/$design_name.tmp/gv_sha256_v1_0_project $repo_dir/gv_sha256_1_0/component.xml
add_files -fileset [current_fileset] -force -copy_to $repo_dir/gv_sha256_1_0/hdl $gv_sha256_src
update_compile_order -fileset [current_fileset]

ipx::merge_project_changes files [ipx::current_core]
ipx::merge_project_changes ports [ipx::current_core]
ipx::update_source_project_archive -component [ipx::current_core]
ipx::create_xgui_files [ipx::current_core]
ipx::update_checksums [ipx::current_core]
ipx::check_integrity [ipx::current_core]
ipx::save_core [ipx::current_core]
ipx::move_temp_component_back -component [ipx::current_core]
close_project -delete
update_ip_catalog -rebuild -repo_path $repo_dir/gv_sha256_1_0


################################################################
# Create platform.
################################################################
source $script_dir/platformdesign.tcl
create_root_design ""

make_wrapper -files [get_files $proj_src/sources_1/bd/$design_name/$design_name.bd] -top
add_files -norecurse $proj_dir/$design_name.gen/sources_1/bd/$design_name/hdl/EmnessSha256_wrapper.vhd

################################################################
# Generate bitstream.
################################################################
launch_runs impl_1 -to_step write_bitstream -jobs 8
wait_on_runs impl_1

write_hw_platform -fixed -include_bit -force -file $out_dir/$design_name.xsa