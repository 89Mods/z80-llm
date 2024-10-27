# What?

Z80-LLM is a bunch of code meant to enable a Z80 CPU to run a GPT-2-Medium sized LLM, but larger should theoretically be possible. This will hopefully finally bring modern AI into the retro-computing era. The code is written in simple C and should be easily portable to platforms not just based on the Z80, but also other retro CPUs with a C compiler toolchain.

# Why?

# How?

## Hardware

The demo hardware consist only of a Z80, 32KiB of ROM and some SRAM. 32KiB is not enough, so the final 16KiB of the address range is paged 16-ways to get a bit more. Though technically less memory usage is possible at the cost of speed. Obviously, running a LLM requires still more memory, so two PIO chips drive a PATA interface to a disk drive, where data is paged in and out of raw disk blocks to get even more space for matrices and vectors. Part of the disk is formated, though, to hold the network parameters.

Two UARTs are provided: one for standard output and a second to print a verbose log.

The schematics and PCB layout files can be found in the folder `Hardware`.

## LLM

The code to pre-train the LLM was written using libtorch in C++, using custom code for implementation of the attention layers, to make translating it into standalone C easier. This code was then supplied with a dataset of book texts and trained on a cloud compute instance containing a RTX4090 that was fed a large budget of 27.16$. The resulting LLM is good enough at generating basic, coherent text for the purposes of demonstration.

This code can be found in the folder `libtorch_llm`.

## Standalone C program

I started out by writing a standalone, single file C program on my desktop PC that can run the LLM, to have a working reference point when writing the code for the Z80.
Still, this program already contains key aspects of what is required on the Z80, such as parsing parameters from files and emulating matrices being stored in a block device.
The resulting code only depends on the standard library and compiles into a tiny executable on Linux.

This code can be found in the folder `c_pgm`.

## Z80 Firmware

z88dk was used to create the program to run the LLM on a Z80. It works as described above by using space on a connected disk drive as memory for matrices and the paged SRAM as cache for that memory. To load the parameters, a filesystem partition is also present on the disk. My choice of filesystem is, of course, ext4, to further modernize the software design. Only basic filesystem read access is supported, but code to handle checksum checking and symlink parsing is present and can be enabled, if required. This allows easy copying and modification of the network parameters on the disk from any common Linux distro!

Standard IEEE 32-bit floats are used for computation. I did experiment with 16-bit floats and fixed-point numbers, but they produced completely broken results due to precission errors. So floats are used, for maximum compatibility! Though, one optimization I did make use of is LUTs. Notably, the GeLU nonlinearity is implemented through a LUT in a file, which I found to be faster.

A byte-pair-encoder almost identical to the one used in GPT-2 is used for tokenization. I will admit, the code I wrote for it is not the best, but it successfully runs on the Z80, so I am happy with it. It also uses data stored on the filesystem to do the encoding.

Note: the resulting code does not fit into the 32KiB ROM. To work around this, the CPU copies some blocks from the disk to SRAM at bootup, allowing code to overflow into RAM. Any code changes require not only re-programming of the ROM, but also an update of the disk image. I apologize for this inconvenience.

This code can be found in the folder `Z80`.

# Speed

At 8MHz and using a SSD as the disk drive for maximum throughput, I have approximated, using simulation, a time of 1,292,518 minutes per token, or roughly 2.4 years. Upgrading to a 20MHz Z80 should cut that down to 1 year, and I am investigating speeds using a AM9511 to accelerate the floating-point computations.

# Usage

The intended method for getting this beast running is to put together a disk image on your PC, and then dd-ing it to a real drive. Open a terminal at the root of this repository.

First, create a blank file that is less than or equal in capacity to your target physical drive. I am using a SSD with slightly less than 16GiB capacity, so I will use a 3G large image: `dd if=/dev/zero of=./disk.img bs=1M count=3072`

Next, it needs a filesystem partition. It can’t span the whole disk, as the program requires a bit of free space after the filesystem partition, which it will use as memory. Use fdisk for this: `fdisk ./disk.img`

Create a MBR partition table (`o`) and add a new partition (`n`), which is to be primary (`p`) and number 1 (`1`). First sector has to be 2048, as the first blocks of the disk past the MBR will store Z80 bytecode that is overflowing from ROM into RAM. For the size of the partition, I go with 2G (`+2G`) so mkfs doesn’t make a fuzz on the next step.

`w` will tell fdisk to write changes to the disk image and exit. To continue, the disk image needs to be set up as a loop device, so it can be worked on directly: `sudo losetup -fP disk.img`. For the next steps, make sure you are targeting the correct device, or you may corrupt a part of your OS! `lsblk` shows one new device, `loop0`, after the previous command, so that has to be it.

Simply doing `sudo mkfs.ext4 /dev/loop0p1` will create the ext4 file system on the partition that was just created. Note: it is possible your distro’s mkfs config enables filesystem features not supported by my code. If this happens, you can comment out the FS feature check in `Z80/ext4.c`. In most cases, it will still work.

At this point, you should be able to minimize the terminal window and continue in your DE’s file browser, as the freshly formatted partition should show up there, like a removable drive. Open it. You can change the permissions to make these next steps easier if you want, it won’t affect things on the Z80.

For now, I will only describe how to install the weights from my pre-trained model. Download the files [here](https://files.tholin.dev/Public/model.tar.gz) and extract into the root of the partition.

Next is the data files for the tokenizer. Navigate to `libtorch_llm/data/` directory and simply run the `prepare_vocab.sh` bash script. I decided to use GPT-2’s actual tokenizer configs, and this script will download them and apply any required modifications. You can then copy the entire `data` folder to the root of the ext4 partition.

Note: you will need nodejs installed to run one of the helper scripts. Yes, I am aware of how absolutely insane the language spread of this repository is.

Once done, the root of the partition should have three entries `LUTs`, `model` and `data`. You can now un-mount the partition. To remove the loop device, I like to use this handy command, which destroys all loop devices associated with a specific file: `losetup -j ./disk.img | grep -o "/dev/loop[0-9][0-9]*" | xargs -L1 sudo losetup -d`.

Now, finally, comes the step of building the Z80 code. Start by installing [z88dk](https://github.com/z88dk/z88dk/) in whichever way you prefer. Then, navigate a terminal to the `Z80` directory in this repo and simply do `make`. The makefile should do everything for you, including modifying the disk image with the overflowing binary (assuming you created it in the right place).

Lastly, get out whatever ROM programmer you use and burn `EPROM.bin` onto a memory chip of your choice. In parallel, start copying the disk image to a real drive, preferably through dd.

Congratulations! You are now ready to wait 2.5 years for your Z80 to say a single word to you!
