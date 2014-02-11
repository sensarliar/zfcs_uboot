/*
 * Copyright (c) 2010, The Android Open Source Project.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Neither the name of The Android Open Source Project nor the names
 *    of its contributors may be used to endorse or promote products
 *    derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <common.h>
#include <mmc.h>
#include <fastboot.h>

#define EFI_VERSION 0x00010000
#define EFI_ENTRIES 128
#define EFI_NAMELEN 36

static const u8 partition_type[16] = {
	0xa2, 0xa0, 0xd0, 0xeb, 0xe5, 0xb9, 0x33, 0x44,
	0x87, 0xc0, 0x68, 0xb6, 0xb7, 0x26, 0x99, 0xc7,
};

static const u8 random_uuid[16] = {
	0xff, 0x1f, 0xf2, 0xf9, 0xd4, 0xa8, 0x0e, 0x5f,
	0x97, 0x46, 0x59, 0x48, 0x69, 0xae, 0xc3, 0x4e,
};

struct efi_entry {
	u8 type_uuid[16];
	u8 uniq_uuid[16];
	u64 first_lba;
	u64 last_lba;
	u64 attr;
	u16 name[EFI_NAMELEN];
};

struct efi_header {
	u8 magic[8];

	u32 version;
	u32 header_sz;

	u32 crc32;
	u32 reserved;

	u64 header_lba;
	u64 backup_lba;
	u64 first_lba;
	u64 last_lba;

	u8 volume_uuid[16];

	u64 entries_lba;

	u32 entries_count;
	u32 entries_size;
	u32 entries_crc32;
} __attribute__((packed));

struct ptable {
	u8 mbr[512];
	union {
		struct efi_header header;
		u8 block[512];
	};
	struct efi_entry entry[EFI_ENTRIES];
};

static void init_mbr(u8 *mbr, u32 blocks)
{
	mbr[0x1be] = 0x00; /* nonbootable */
	mbr[0x1bf] = 0xFF; /* bogus CHS */
	mbr[0x1c0] = 0xFF;
	mbr[0x1c1] = 0xFF;

	mbr[0x1c2] = 0xEE; /* GPT partition */
	mbr[0x1c3] = 0xFF; /* bogus CHS */
	mbr[0x1c4] = 0xFF;
	mbr[0x1c5] = 0xFF;

	mbr[0x1c6] = 0x01; /* start */
	mbr[0x1c7] = 0x00;
	mbr[0x1c8] = 0x00;
	mbr[0x1c9] = 0x00;

	memcpy(mbr + 0x1ca, &blocks, sizeof(u32));

	mbr[0x1fe] = 0x55;
	mbr[0x1ff] = 0xaa;
}

static void start_ptbl(struct ptable *ptbl, unsigned blocks)
{
	struct efi_header *hdr = &ptbl->header;

	memset(ptbl, 0, sizeof(*ptbl));

	init_mbr(ptbl->mbr, blocks - 1);

	memcpy(hdr->magic, "EFI PART", 8);
	hdr->version = EFI_VERSION;
	hdr->header_sz = sizeof(struct efi_header);
	hdr->header_lba = 1;
	hdr->backup_lba = blocks - 1;
	hdr->first_lba = 34;
	hdr->last_lba = blocks - 1;
	memcpy(hdr->volume_uuid, random_uuid, 16);
	hdr->entries_lba = 2;
	hdr->entries_count = EFI_ENTRIES;
	hdr->entries_size = sizeof(struct efi_entry);
}

static void end_ptbl(struct ptable *ptbl)
{
	struct efi_header *hdr = &ptbl->header;
	u32 n;

	n = crc32(0, 0, 0);
	n = crc32(n, (void *) ptbl->entry, sizeof(ptbl->entry));
	hdr->entries_crc32 = n;

	n = crc32(0, 0, 0);
	n = crc32(0, (void *) &ptbl->header, sizeof(ptbl->header));
	hdr->crc32 = n;
}

int add_ptn(struct ptable *ptbl, u64 first, u64 last, const char *name)
{
	struct efi_header *hdr = &ptbl->header;
	struct efi_entry *entry = ptbl->entry;
	unsigned n;

	if (first < 34) {
		printf("partition '%s' overlaps partition table\n", name);
		return -1;
	}

	if (last > hdr->last_lba) {
		printf("partition '%s' does not fit\n", name);
		return -1;
	}
	for (n = 0; n < EFI_ENTRIES; n++, entry++) {
		if (entry->last_lba)
			continue;
		memcpy(entry->type_uuid, partition_type, 16);
		memcpy(entry->uniq_uuid, random_uuid, 16);
		entry->uniq_uuid[0] = n;
		entry->first_lba = first;
		entry->last_lba = last;
		for (n = 0; (n < EFI_NAMELEN) && *name; n++)
			entry->name[n] = *name++;
		return 0;
	}
	printf("out of partition table entries\n");
	return -1;
}

void import_efi_partition(struct efi_entry *entry)
{
	struct fastboot_ptentry e;
	int n;
	if (memcmp(entry->type_uuid, partition_type, sizeof(partition_type)))
		return;
	for (n = 0; n < (sizeof(e.name)-1); n++)
		e.name[n] = entry->name[n];
	e.name[n] = 0;
	e.start = entry->first_lba;
	e.length = (entry->last_lba - entry->first_lba + 1) * 512;
	e.flags = 0;

	if (!strcmp(e.name, "environment"))
		e.flags |= FASTBOOT_PTENTRY_FLAGS_WRITE_ENV;
	fastboot_flash_add_ptn(&e);

	if (e.length > 0x100000)
		printf("%8d %7dM %s\n", e.start, e.length/0x100000, e.name);
	else
		printf("%8d %7dK %s\n", e.start, e.length/0x400, e.name);
}

static int load_ptbl(void)
{
	static unsigned char data[512];
	static struct efi_entry entry[4];
	int n, m;
	char source[32], dest[32], length[32];

	char *mmc_read[5]  = {"mmc", "read", NULL, NULL, NULL};

	/* read mbr */
	mmc_read[2] = source;
	mmc_read[3] = dest;
	mmc_read[4] = length;

	sprintf(source, "0x%x", data);
	sprintf(dest, "0x%x", 0x1);
	sprintf(length, "0x%x", 1);

	if (do_mmcops(NULL, 0, 5, mmc_read)) {
		printf("Reading boot magic FAILED!\n");
		return -1;
	}

	if (memcmp(data, "EFI PART", 8)) {
		printf("efi partition table not found\n");
		return -1;
	}
	for (n = 0; n < (128/4); n++) {

		/* read partition */
		source[0] = '\0';
		dest[0] = '\0';
		length[0] = '\0';
		mmc_read[2] = source;
		mmc_read[3] = dest;
		mmc_read[4] = length;

		sprintf(source, "0x%x", entry);
		sprintf(dest, "0x%x", 0x1+n);
		sprintf(length, "0x%x", 1);

		if (do_mmcops(NULL, 0, 5, mmc_read)) {
			printf("Reading boot magic FAILED!\n");
			return -1;
		}
		for (m = 0; m < 4; m++)
			import_efi_partition(entry + m);
	}
	return 0;
}

struct partition {
	const char *name;
	unsigned size_kb;
};

/* eMMC partition layout (All sizes are in kB)
 * Modify the below partition table to change the GPT configuration.
 * The entry for each partition can be modified as per the requirement.
 */
static struct partition partitions[] = {
	{ "-", 128 },			/* Master Boot Record and GUID Partition Table */
	{ "spl", 128 },			/* First stage bootloader */
	{ "bootloader", 512 },		/* Second stage bootloader */
	{ "misc", 128 },		/* Rserved for internal purpose */
	{ "-", 128 },			/* Reserved */
	{ "recovery", 8*1024 },		/* Recovery partition  */
	{ "boot", 8*1024 },		/* Partition contains kernel + ramdisk images */
	{ "system", 256*1024 },		/* Android file system */
	{ "cache", 256*1024 },		/* Store Application Cache */
	{ "userdata", 256*1024 },	/* User data */
	{ "media", 0 },			/* Media files */
	{ 0, 0 },
};

static struct ptable the_ptable;

static int do_format(void)
{
	struct ptable *ptbl = &the_ptable;
	unsigned sector_sz, blocks;
	unsigned next;
	int n;

	printf("\ndo_format ..!!");
	/* get mmc info */
	struct mmc *mmc = find_mmc_device(get_mmc_cur_dev());
	if (mmc == 0) {
		printf("no mmc device at slot 0");
		return -1;
	}

	mmc->has_init = 0;
	if (mmc_init(mmc)) {

		printf("\n mmc init FAILED");
		return -1;
	} else{
		printf("\nmmc capacity is:0x%x", mmc->capacity);
		printf("\nmmc: number of blocks:0x%x", mmc->block_dev.lba);
		printf("\nmmc: block size:0x%x", mmc->block_dev.blksz);
	}

	blocks = mmc->block_dev.lba;
	sector_sz = mmc->block_dev.blksz;

	start_ptbl(ptbl, blocks);
	n = 0;
	next = 0;
	for (n = 0, next = 0; partitions[n].name; n++) {
		/* 10/11 : below line change size from KB to no of blocks */
		unsigned sz = partitions[n].size_kb*2 ;
		if (!strcmp(partitions[n].name, "-")) {
			next += sz;
			continue;
		}
		if (sz == 0)
			sz = blocks - next;
		if (add_ptn(ptbl, next, next + sz - 1, partitions[n].name))
			return -1;
		next += sz;
	}
	end_ptbl(ptbl);

	fastboot_flash_reset_ptn();

	/* 10/11:modified as per PSP release support */
	char *mmc_write[5]  = {"mmc", "write", NULL, NULL, NULL};
	char source[32], dest[32], length[32];

	mmc_write[2] = source;
	mmc_write[3] = dest;
	mmc_write[4] = length;

	sprintf(source, "0x%x", (void *)ptbl);
	sprintf(dest, "0x%x", 0x00);
	sprintf(length, "0x%x", (sizeof(struct ptable)/512)+1);

	if (do_mmcops(NULL, 0, 5, mmc_write)) {
		printf("Writing mbr is FAILED!\n");
		return -1;
	} else {
		printf("Writing mbr is DONE!\n");
	}

	printf("\nnew partition table:\n");
	load_ptbl();

	return 0;
}

int fastboot_oem(const char *cmd)
{
	printf("fastboot_oem:%s", cmd);
	if (!strcmp(cmd, "format"))
		return do_format();
	return -1;
}

void board_select_mmc(void)
{
	char *mmc_sel[3] = {"mmc", "dev", "0",};
#ifdef CONFIG_USE_MMC0
	mmc_sel[2] = "0";
#endif
#ifdef CONFIG_USE_MMC1
	mmc_sel[2] = "1";
#endif

	if (do_mmcops(NULL, 0, 3, mmc_sel)) {
		printf("FAIL: Unable to select MMC device\n");
	}
}

int board_mmc_fbtptn_init(void)
{
	char *mmc_init[2] = {"mmc", "rescan",};

	if (do_mmcops(NULL, 0, 2, mmc_init)) {
		printf("FAIL:Init of MMC card");
		return 1;
	}

	printf("\nefi partition table:\n");
	return load_ptbl();
}
