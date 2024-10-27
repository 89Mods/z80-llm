# What?

Z80-LLM is a bunch of code meant to enable a Z80 CPU to run a GPT-2-Medium sized LLM, but larger should theoretically be possible. This will hopefully finally bring modern AI into the retro-computing era. The code is written in simple C and should be easily portable to platforms not just based on the Z80, but also other retro CPUs with a C compiler toolchain.

# Why?

# How?

## Hardware

The demo hardware consist only of a Z80, 32KiB of ROM and some SRAM. 32KiB is not enough, so the final 16KiB of the address range is paged 16-ways to get a bit more. Though technically less memory usage is possible at the cost of speed. Obviously, running a LLM requires still more memory, so two PIO chips drive a PATA interface to a disk drive, where data is paged in and out of raw disk blocks to get even more space for matrices and vectors. Part of the disk is formated, though, to hold the network parameters.

Two UARTs are provided: one for standard output and a second to print a verbose log.

The schematics and PCB layout files can be found in the folder (TODO)

## LLM

The code to pre-train the LLM was written using libtorch in C++, using custom code for implementation of the attention layers, to make translating it into standalone C easier. This code was then supplied with a dataset of book texts and trained on a cloud compute instance containing a RTX4090 that was fed a large budget of 27.16$. The resulting LLM is good enough at generating basic, coherent text for the purposes of demonstration.

This code can be found in the folder (TODO)

## Standalone C program

I started out by writing a standalone, single file C program on my desktop PC that can run the LLM, to have a working reference point when writing the code for the Z80.
Still, this program already contains key aspects of what is required on the Z80, such as parsing parameters from files and emulating matrices being stored in a block device.
The resulting code only depends on the standard library and compiles into a tiny executable on Linux.

This code can be found in the folder (TODO)

## Z80 Firmware

z88dk was used to create the program to run the LLM on a Z80. It works as described above by using space on a connected disk drive as memory for matrices and the paged SRAM as cache for that memory. To load the parameters, a filesystem partition is also present on the disk. My choice of filesystem is, of course, ext4, to further modernize the software design. Only basic filesystem read access is supported, but code to handle checksum checking and symlink parsing is present and can be enabled, if required. This allows easy copying and modification of the network parameters on the disk from any common Linux distro!

Standard IEEE 32-bit floats are used for computation. I did experiment with 16-bit floats and fixed-point numbers, but they produced completely broken results due to precission errors. So floats are used, for maximum compatibility! Though, one optimization I did make use of is LUTs. Notably, the GeLU nonlinearity is implemented through a LUT in a file, which I found to be faster.

A byte-pair-encoder almost identical to the one used in GPT-2 is used for tokenization. I will admit, the code I wrote for it is not the best, but it successfully runs on the Z80, so I am happy with it. It also uses data stored on the filesystem to do the encoding.

Note: the resulting code does not fit into the 32KiB ROM. To work around this, the CPU copies some blocks from the disk to SRAM at bootup, allowing code to overflow into RAM. Any code changes require not only re-programming of the ROM, but also an update of the disk image. I apologize for this inconvenience.

This code can be found in the folder (TODO)

# Speed

At 8MHz and using a SSD as the disk drive for maximum throughput, I have approximated, using simulation, a time of 1,292,518 minutes per token, or roughly 2.4 years. Upgrading to a 20MHz Z80 should cut that down to 1 year, and I am investigating speeds using a AM9511 to accelerate the floating-point computations.

# Usage

Note: this section covers setting up using my pre-trained model, which can be gotten from (LINK TODO)

TODO
