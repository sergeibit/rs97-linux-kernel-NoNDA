/*
 *  nanddump.c
 *
 *  Copyright (C) 2000 David Woodhouse (dwmw2@infradead.org)
 *                     Steven J. Hill (sjhill@realitydiluted.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Overview:
 *   This utility dumps the contents of raw NAND chips or NAND
 *   chips contained in DoC devices.
 */

#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <asm/types.h>
#include <mtd/mtd-user.h>

#define PROGRAM "nanddump"
#define VERSION "$Revision: 1.1.1.1 $"

struct nand_oobinfo none_oobinfo = {
	.useecc = MTD_NANDECC_OFF,
};

void display_help (void)
{
	printf("Usage: nanddump [OPTIONS] MTD-device\n"
			"Dumps the contents of a nand mtd partition.\n"
			"\n"
			"           --help	        display this help and exit\n"
			"           --version	        output version information and exit\n"
			"-f file    --file=file          dump to file\n"
			"-i         --ignoreerrors       ignore errors\n"
			"-l length  --length=length      length\n"
			"-n         --noecc              read without error correction\n"
			"-o         --omitoob            omit oob data\n"
			"-b         --omitbad            omit bad blocks from the dump\n"
			"-p         --prettyprint        print nice (hexdump)\n"
			"-s addr    --startaddress=addr  start address\n");
	exit(0);
}

void display_version (void)
{
	printf(PROGRAM " " VERSION "\n"
			"\n"
			PROGRAM " comes with NO WARRANTY\n"
			"to the extent permitted by law.\n"
			"\n"
			"You may redistribute copies of " PROGRAM "\n"
			"under the terms of the GNU General Public Licence.\n"
			"See the file `COPYING' for more information.\n");
	exit(0);
}

// Option variables

int	ignoreerrors;		// ignore errors
int	pretty_print;		// print nice in ascii
int	noecc;			// don't error correct
int	omitoob;		// omit oob data
unsigned long	start_addr;	// start address
unsigned long	length;		// dump length
char    *mtddev;		// mtd device name
char    *dumpfile;		// dump file name
int	omitbad;

void process_options (int argc, char *argv[])
{
	int error = 0;

	for (;;) {
		int option_index = 0;
		static const char *short_options = "bs:f:il:opn";
		static const struct option long_options[] = {
			{"help", no_argument, 0, 0},
			{"version", no_argument, 0, 0},
			{"file", required_argument, 0, 'f'},
			{"ignoreerrors", no_argument, 0, 'i'},
			{"prettyprint", no_argument, 0, 'p'},
			{"omitoob", no_argument, 0, 'o'},
			{"omitbad", no_argument, 0, 'b'},
			{"startaddress", required_argument, 0, 's'},
			{"length", required_argument, 0, 'l'},
			{"noecc", no_argument, 0, 'n'},
			{0, 0, 0, 0},
		};

		int c = getopt_long(argc, argv, short_options,
				long_options, &option_index);
		if (c == EOF) {
			break;
		}

		switch (c) {
			case 0:
				switch (option_index) {
					case 0:
						display_help();
						break;
					case 1:
						display_version();
						break;
				}
				break;
			case 'b':
				omitbad = 1;
				break;
			case 's':
				start_addr = strtol(optarg, NULL, 0);
				break;
			case 'f':
				if (!(dumpfile = strdup(optarg))) {
					perror("stddup");
					exit(1);
				}
				break;
			case 'i':
				ignoreerrors = 1;
				break;
			case 'l':
				length = strtol(optarg, NULL, 0);
				break;
			case 'o':
				omitoob = 1;
				break;
			case 'p':
				pretty_print = 1;
				break;
			case 'n':
				noecc = 1;
				break;
			case '?':
				error = 1;
				break;
		}
	}

	if ((argc - optind) != 1 || error)
		display_help ();

	mtddev = argv[optind];
}

/*
 * Buffers for reading data from flash
 */
unsigned char readbuf[8192];
unsigned char oobbuf[256];

/*
 * Main program
 */
int main(int argc, char **argv)
{
	unsigned long ofs, end_addr = 0;
	unsigned long long blockstart = 1;
	int ret, fd, ofd, bs, badblock = 0;
	struct mtd_oob_buf oob = {0, 16, oobbuf};
	mtd_info_t meminfo;
	int oobinfochanged = 0 ;
	struct nand_oobinfo old_oobinfo;
	struct mtd_ecc_stats stat1, stat2;
	int eccstats = 0;

	process_options(argc, argv);

	/* Open MTD device */
	if ((fd = open(mtddev, O_RDONLY)) == -1) {
		perror("open flash");
		exit (1);
	}

	/* Fill in MTD device capability structure */
	if (ioctl(fd, MEMGETINFO, &meminfo) != 0) {
		perror("MEMGETINFO");
		close(fd);
		exit (1);
	}

	/* Make sure device page sizes are valid */
	if (!(meminfo.oobsize == 256 && meminfo.writesize == 8192) &&
	    !(meminfo.oobsize == 128 && meminfo.writesize == 4096) &&
	    !(meminfo.oobsize == 64 && meminfo.writesize == 2048) &&
	    !(meminfo.oobsize == 32 && meminfo.writesize == 1024) &&
	    !(meminfo.oobsize == 16 && meminfo.writesize == 512) &&
	    !(meminfo.oobsize == 8 && meminfo.writesize == 256)) {
		fprintf(stderr, "Unknown flash (not normal NAND)\n");
		close(fd);
		exit(1);
	}
	/* Read the real oob length */
	oob.length = meminfo.oobsize;

	if (noecc)  {
		ret = ioctl(fd, MTDFILEMODE, (void *) MTD_MODE_RAW);
		if (ret == 0) {
			oobinfochanged = 2;
		} else {
			switch (errno) {
			case ENOTTY:
				if (ioctl (fd, MEMGETOOBSEL, &old_oobinfo) != 0) {
					perror ("MEMGETOOBSEL");
					close (fd);
					exit (1);
				}
				if (ioctl (fd, MEMSETOOBSEL, &none_oobinfo) != 0) {
					perror ("MEMSETOOBSEL");
					close (fd);
					exit (1);
				}
				oobinfochanged = 1;
				break;
			default:
				perror ("MTDFILEMODE");
				close (fd);
				exit (1);
			}
		}
	} else {

		/* check if we can read ecc stats */
		if (!ioctl(fd, ECCGETSTATS, &stat1)) {
			eccstats = 1;
			fprintf(stderr, "ECC failed: %d\n", stat1.failed);
			fprintf(stderr, "ECC corrected: %d\n", stat1.corrected);    
			fprintf(stderr, "Number of bad blocks: %d\n", stat1.badblocks);    
			fprintf(stderr, "Number of bbt blocks: %d\n", stat1.bbtblocks);    
		} else
			perror("No ECC status information available");
	}

	/* Open output file for writing. If file name is "-", write to standard
	 * output. */
	if (!dumpfile) {
		ofd = STDOUT_FILENO;
	} else if ((ofd = open(dumpfile, O_WRONLY | O_TRUNC | O_CREAT, 0644))== -1) {
		perror ("open outfile");
		close(fd);
		exit(1);
	}

	/* Initialize start/end addresses and block size */
	if (length)
		end_addr = start_addr + length;
	if (!length || end_addr > meminfo.size)
		end_addr = meminfo.size;

	bs = meminfo.writesize;

	/* Print informative message */
	fprintf(stderr, "Block size %u, page size %u, OOB size %u\n",
			meminfo.erasesize, meminfo.writesize, meminfo.oobsize);
	fprintf(stderr,
			"Dumping data starting at 0x%08x and ending at 0x%08x...\n",
			(unsigned int) start_addr, (unsigned int) end_addr);

	/* Dump the flash contents */
	for (ofs = start_addr; ofs < end_addr ; ofs+=bs) {

		// new eraseblock , check for bad block
		if (blockstart != (ofs & (~meminfo.erasesize + 1))) {
			blockstart = ofs & (~meminfo.erasesize + 1);
			if ((badblock = ioctl(fd, MEMGETBADBLOCK, &blockstart)) < 0) {
				perror("ioctl(MEMGETBADBLOCK)");
				goto closeall;
			}
		}

		if (badblock) {
			//skip bad block;
			ofs += meminfo.erasesize - bs;
			continue;		
		} else {
			/* Read page data and exit on failure */
			if (pread(fd, readbuf, bs, ofs) != bs) {
				perror("pread");
				goto closeall;
			}
		}

		/* ECC stats available ? */
		if (eccstats) {
			if (ioctl(fd, ECCGETSTATS, &stat2)) {
				perror("ioctl(ECCGETSTATS)");
				goto closeall;
			}
			if (stat1.failed != stat2.failed)
				fprintf(stderr, "ECC: %d uncorrectable bitflip(s)"
						" at offset 0x%08lx\n",
						stat2.failed - stat1.failed, ofs);
			if (stat1.corrected != stat2.corrected)
				fprintf(stderr, "ECC: %d corrected bitflip(s) at"
						" offset 0x%08lx\n",
						stat2.corrected - stat1.corrected, ofs);
			stat1 = stat2;
		}

		if (badblock) {
			memset (readbuf, 0xff, meminfo.oobsize);
		} else {
			/* Read OOB data and exit on failure */
			oob.start = ofs;
			if (ioctl(fd, MEMREADOOB, &oob) != 0) {
				perror("ioctl(MEMREADOOB)");
				goto closeall;
			}
			if(oobbuf[2]==0xff && oobbuf[3]==0xff && oobbuf[4]==0xff && oobbuf[5]==0xff){
				//skip free block;
				ofs += meminfo.erasesize - bs;
				continue;	
			}
		}

		/* Write out page data */
		write(ofd, readbuf, bs);
		
		if (omitoob)
			continue;
		
		/* Write out OOB data */
		write(ofd, oobbuf, meminfo.oobsize);
	}

	/* reset oobinfo */
	if (oobinfochanged == 1) {
		if (ioctl (fd, MEMSETOOBSEL, &old_oobinfo) != 0) {
			perror ("MEMSETOOBSEL");
			close(fd);
			close(ofd);
			return 1;
		}
	}
	/* Close the output file and MTD device */
	close(fd);
	close(ofd);

	/* Exit happy */
	return 0;

closeall:
	/* The new mode change is per file descriptor ! */
	if (oobinfochanged == 1) {
		if (ioctl (fd, MEMSETOOBSEL, &old_oobinfo) != 0)  {
			perror ("MEMSETOOBSEL");
		}
	}
	close(fd);
	close(ofd);
	exit(1);
}
