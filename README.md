# Red-Pitaya-Levitodynamics-Feedback-Project
Description: Red Pitaya FPGA feedback project to control (in this case, stabilize or "cool") levitated particles in a trap. It was designed for using electrodes to cool charged optically levitated nanoparticles, but it can work in other types of oscillators with no or minor modifications (for instance, charged particles in Paul traps). The FPGA bitstream is generated with Vivado and comes with a set of C code to control the FPGA parameters from the CPU part of the Red Pitaya (working from the Red Pitaya terminal). The C routine manages the LQR controller parameters, the feedback delay and an optional adaptive loop that automatically optimizes the feedback parameter k_d with a gradient descent algorithm.

For an in-depth description on how it was used and meaning of parameters, read Ref: https://journals.aps.org/prl/abstract/10.1103/PhysRevLett.122.223602 (in particular, check the Supplemental Material). If you use this code (or a modified version of it) in a paper, I ask that you cite 
> Conangla, Gerard P., et al. "Optimal Feedback Cooling of a charged Levitated Nanoparticle with Adaptive Control." Physical Review Letters 122.22 (2019): 223602.
in the references section.

For any other comments or questions, drop me a message.


Guide on what to do to get started with the Red Pitaya. It is in the order you should follow to get things working. You should exactly use the versions specified in this guide, otherwise I can't assure that it will work.
--------------


1 Get a Red Pitaya!
--------------
Quite clear what to do.

2 Install fresh red pitaya image
--------------
Windows:
Download the image redpitaya_ubuntu_17-11-35_01-sep-2017.img, install to a microSD with win32diskimager. If the version is not this, I don’t guarantee that any of the following will work. Also, make sure that the microSD is of 32GB or less. Otherwise it will (probably) not work.


3 Connect to the Red Pitaya (RP):
--------------
I recommend creating a local network with a switch, connected to an extra network card in the computer. 

Use this IP for the LAN: router/switch gateway 192.168.1.1, mask 255.255.255.0
Connect to the RP by typing “rpxxxxxx.local/” (where xxxxxx are the last 6 digits of the RP MAC address), then change IP to static with:

system > network manager > Wired connection 
status: Static, IP = 192.168.1.X,

where X is (for instance) the last number of the MAC address of the RP stemlab board (or whatever other number you like). 
To actually work inside the RP, you will need to connect from a terminal. How to do it in…:

Ubuntu
With ssh: 
> ssh root@rp_ip_address
Password: root

You should be inside the redpitaya, that runs a version of Linux. Hence, you can do anything you would do on a Linux machine (like, e.g., compiling C code with gcc):

> gcc some_code.c -o some_code.o

Windows
Like in ubuntu, but use Putty (https://www.putty.org/).


4 Programming with the RP
--------------
From now on it will be assumed that we work on Ubuntu. This section is ONLY REQUIRED IF YOU WANT TO MODIFY THE FPGA CODE. If not the case, you can just download the standard bistream file and load it to the Red Pitaya.

Make sure the RP has an IP assigned. Then:

Install Vivado 2017.2 with SDK and WEBpackage for zynq7000 in Ubuntu 18.04 LTS. If you want to create a new project: 
- Open vivado -> create new project -> During the project creation the script specifies Red Pitaya’s FPGA part name xc7z010clg400-1 (it's fundamental to select this one, otherwise who knows what will happen).
- If not, you can start working by modifying some already working example, such as the feedback code provided here. A nice guide (which I used instensively) is this http://antonpotocnik.com/?p=487360. It’s a good idea to read it in detail or at least take a good look.
In design > processing system we see the zynq important block: fpga and ports. Fclk_cl0 is the clock.

Check how connectors point somewhere. This tells you if that is an input, output, or it is bidirectional.
To compile the code and generate a bitstream, click on "Generate Bitstream".
To save the Vivado project (in an autocontained file without external dependencies), go to Vivavdo File -> Project -> Archive, selecting the desired options.


5 Loading bitstream and controlling the RP from the CPU with a C routine
--------------
C code should always be compiled IN THE REDPITAYA, and also executed there. Once we have compiled our Vivado project and we have a bitstream (.bit) file. We need to copy this file to the redpitaya with (the folder below is just an example, but they all follow the same structure):

> cd redpitaya_guide/tmp/1_projectname/1_projectname.runs/impl_1/
> scp system_wrapper.bit root@your_rp_ip:whatever_name_you_like.bit

Now connect to red pitaya

> ssh root@192.168.1.3 (pw = “root”)

To load our bitstream to the FPGA (as opposed to the default RP program that loads once you access the RP from the browser): assume it is located in the /root/ folder of the Red Pitaya:

> cat /root/led_blink.bit > /dev/xdevcfg

After this it should start running whatever we have programmed. To now run the C routine to control whatever parameters are accessible in the FPGA, first compile it with

> gcc some_code.c -o some_code.o -lm

And then run it with

> ./some_code.o 

If you want to roll back to the normal red pitaya bitstream type:

> cat /opt/redpitaya/fpga/fpga_X.XX.bit > /dev/xdevcfg

Where fpga_X.XX is the current version, whatever it is. Reinitializing the red pitaya also works.
