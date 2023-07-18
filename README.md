# SHA-256 Core Linux Driver on a ZYNQ APSoC 

Open access lab experience as an introduction to the development of
Linux/FreeRTOS device drivers for hardware accelerators.

The idea comes from the [EMNESS](https://emness.eu) initiative, with the goal
of structuring innovative academic curricula on reliability and hardware
security.

<img src="doc/emness_logo.png" width="100">  

## Contents

* [What is this?](#what-is-this)

* [Getting started](#getting-started)

    * [Generating the Hardware Design](#generating-the-hardware-design)

    * [Baremetal Platform Test](#baremetal-platform-test)

* [Exercises](#exercises)

    * [Exercise 1](#exercise-1)

* [License](#license)

* [Sources](#sources)

* [Conclusion](#conclusion)

## What is this?

## Getting Started
The project targets the [TUL PYNQ-Z2](https://www.tulembedded.com/fpga/ProductsPYNQ-Z2.html) board,
but other all programmable SoC architectures can be used by adapting the flow according to the
vendor's documentation. The steps listed below quickly guide you through the preliminary steps,
from the design of the platform up to the compilation of the loadable kernel module.

Before moving on, make sure to have the following tools available:
* Vivado and Vitis design environments
* Cable Drivers
* PetaLinux
* any Serial Communication Program of your choice

Detailed installation instructions are available in the [documentation portal][amd-doc]; for a
simple installation have a look at the AMD Unified Installer. Notice that on Windows, *Install Cable
Drivers* is an optional selection in the installer; for linux instead, a dedicated script is
provided within Vivado install directory. 

Then, run a clone:

    git clone https://github.com/fscatox/emness_sha256.git

and change into the newly created preject directory:

    cd /path/to/emness_sha256

### Generating the Hardware Design
AMD hardware designs are created with Vivado and exported in the proprietary `xsa` file format,
which can then be used by both Vitis and PetaLinux. The process is automated with a TCL script; 
while in the root project directory, run:

    vivado -source scripts/main_vivado.tcl

The output is generated in the `out/` folder. Notice that the critical warnings concerning the 
`PCW_UIPARAM_DDR_DQS_TO_CLK_DELAY` parameters are hardware errata.

Inside [`src/ip/hdl`](src/ip/hdl) you can examine 
the RTL description of the open source crypto core retrieved from Jonny Doin's 
[project][sha256-core]. Altough it's a stream hash engine, for simplicity it's wrapped as an 
AXI4 peripheral with a Lite interface: this is where the peripheral register file and the 
synchronization mechanisms are implemented. 

After having packaged the IP core, the platform is assembled by picking modules from the Vivado
catalogue and wrapped as the top entity. 

<p align="center">
<img src="doc/platform_diagram.png" width="900">
</p>

### Baremetal Platform Test
Before tackling the linux development problem, a simple baremetal driver and a self-test programs
are available in [`src/ip/api`](src/ip/api). To test the hardware design.
1. Open Vitis and specify a workspace directory.
2. Create a *New Application Project* and choose the tab for creating a new platform from hardware,
   namely `out/EmnessSha256.xsa`. Keep the default selection to generate the boot components

   <p align="center">
   <img src="doc/vitis_newapp.png" width="700">  
   </p>

3. Enter a name for the project and keep the default selection of a single target processor core.
4. Pick the *Empty Application (C)* and finish the wizard.
5. In the *Explorer* tab, under `<project-name>_system/<project-name>` right click on `src` and
   select *Import Sources*
   
   <p align="center">
   <img src="doc/vitis_importsrc.png" width="700">  
   </p>

6. In the *Assistant* tab, select the `<project-name>_system` entry and hit *Run*.

### PetaLinux Flow
The PetaLinux flow is extensively documented in the reference guide [UG1144][amd-doc]. The steps for
a custom hardware platform are summarized in the table below.

| Design Step | Tool |
| ----------- | ---- |
| Create a PetaLinux project | `petalinux-create -t project` |
| Initialize a PetaLinux project (for custom hardware only) | `petalinux-config--get-hw-description` |
| Configure system-level options | `petalinux-config` |
| Create user components | `petalinux-create -t COMPONENT` |
| Configure U-Boot | `petalinux-config -c u-boot` |
| Configure the Linux kernel | `petalinux-config -c kernel` |
| Configure the root filesystem | `petalinux-config -c rootfs` |
| Build the system | `petalinux-build` |
| Package for deploying the system | `petalinux-package` |

## License

[GNU GPLv3](https://choosealicense.com/licenses/gpl-3.0/)

## Sources
[Hash Functions][hash-functions] -

[Secure Hash Standard][sh-standard] -

[SHA-256 Core][sha256-core] - 

[AMD Documentation Portal][amd-doc] - 

[//]: # "Source definitions"
[hash-functions]: https://csrc.nist.gov/projects/hash-functions "Hash Functions by NIST CSRC"
[sh-standard]: http://dx.doi.org/10.6028/NIST.FIPS.180-4 "FIPS PUB 180-4 Secure Hash Standard"
[sha256-core]: https://opencores.org/projects/sha256_hash_core "SHA-256 HASH CORE"
[amd-doc]: https://docs.xilinx.com/ "AMD Documentation Portal"

## Conclusion

