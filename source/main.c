
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <stdio.h>
#include <unistd.h>

#include <net/net.h>

#include <lv2_syscall.h>
#include <lv1_map.h>
#include <udp_printf.h>

#define LV1_VALUE_HIDE				0x39840200f8010090ull
#define LV1_VALUE_SHOW				0x39840000f8010090ull

#define ADDR_355				0x2786E8


/* NAND FLASH */

#define NAND_FLASH_DEV_ID			0x100000000000001ull
#define NAND_FLASH_SECTOR_SIZE			0x200ull
#define NAND_FLASH_START_SECTOR			0x0ull
#define NAND_FLASH_FLAGS			0x22ull

/* NOR FLASH */

#define NOR_FLASH_DEV_ID			0x100000000000004ull
#define NOR_FLASH_SECTOR_SIZE			0x200ull
#define NOR_FLASH_START_SECTOR			0x0ull
#define NOR_FLASH_FLAGS				0x22ull

#define DUMP_FILENAME				"flash.bin"

static const char *dump_path[] = {
	"/dev_usb000/" DUMP_FILENAME,
	"/dev_usb001/" DUMP_FILENAME,
	"/dev_usb002/" DUMP_FILENAME,
	"/dev_usb003/" DUMP_FILENAME,
	"/dev_usb004/" DUMP_FILENAME,
	"/dev_usb005/" DUMP_FILENAME,
	"/dev_usb006/" DUMP_FILENAME,
	"/dev_usb007/" DUMP_FILENAME,
};

/*
 * is_vflash_on
 */
static int is_vflash_on(void)
{
	uint8_t flag;

	lv2_ss_get_cache_of_flash_ext_flag(&flag);

	return !(flag & 0x1);
}

/*
 * open_dump
 */
static FILE *open_dump(void)
{
#define N(a)	(sizeof(a) / sizeof(a[0]))

	FILE *fp;
	int i;

	fp = NULL;

	for (i = 0; i < N(dump_path); i++) {
		PRINTF("%s:%d: trying path '%s'\n", __func__, __LINE__, dump_path[i]);

		fp = fopen(dump_path[i], "w");
		if (fp)
			break;
	}

	if (fp)
		PRINTF("%s:%d: path '%s'\n", __func__, __LINE__, dump_path[i]);
	else
		PRINTF("%s:%d: file could not be opened\n", __func__, __LINE__);

	return fp;

#undef N
}

/*
 * dump_nand_flash
 */
int dump_nand_flash(void)
{
#define NSECTORS	16
	lv1_poke(ADDR_355, LV1_VALUE_SHOW);

	uint32_t dev_handle;
	struct storage_device_info info;
	FILE *fp;
	int start_sector, sector_count;
	uint32_t unknown2;
	uint8_t buf[NAND_FLASH_SECTOR_SIZE * NSECTORS];
	int result;

	dev_handle = 0;
	fp = NULL;

	result = lv2_storage_open(NAND_FLASH_DEV_ID, &dev_handle);
	if (result) {
		PRINTF("%s:%d: lv2_storage_open failed (0x%08x)\n", __func__, __LINE__, result);
		goto done;
	}

	fp = open_dump();
	if (!fp)
		goto done;

	result = lv2_storage_get_device_info(NAND_FLASH_DEV_ID, &info);
	if (result) {
		PRINTF("%s:%d: lv2_storage_get_device_info failed (0x%08x)\n", __func__, __LINE__, result);
		goto done;
	}

	PRINTF("%s:%d: capacity (0x%016llx)\n", __func__, __LINE__, info.capacity);

	start_sector = NAND_FLASH_START_SECTOR;
	sector_count = info.capacity;

	while (sector_count >= NSECTORS) {
		PRINTF("%s:%d: reading data start_sector (0x%08x) sector_count (0x%08x)\n",
			__func__, __LINE__, start_sector, NSECTORS);

		result = lv2_storage_read(dev_handle, 0, start_sector, NSECTORS, buf, &unknown2, NAND_FLASH_FLAGS);
		if (result) {
			PRINTF("%s:%d: lv2_storage_read failed (0x%08x)\n", __func__, __LINE__, result);
			goto done;
		}

		PRINTF("%s:%d: dumping data start_sector (0x%08x) sector_count (0x%08x)\n",
			__func__, __LINE__, start_sector, NSECTORS);

		result = fwrite(buf, 1, NSECTORS * NAND_FLASH_SECTOR_SIZE, fp);
		if (result < 0) {
			PRINTF("%s:%d: fwrite failed (0x%08x)\n", __func__, __LINE__, result);
			goto done;
		}

		start_sector += NSECTORS;
		sector_count -= NSECTORS;
	}

	while (sector_count) {
		PRINTF("%s:%d: reading data start_sector (0x%08x) sector_count (0x%08x)\n",
			__func__, __LINE__, start_sector, 1);

		result = lv2_storage_read(dev_handle, 0, start_sector, 1, buf, &unknown2, NAND_FLASH_FLAGS);
		if (result) {
			PRINTF("%s:%d: lv2_storage_read failed (0x%08x)\n", __func__, __LINE__, result);
			goto done;
		}

		PRINTF("%s:%d: dumping data start_sector (0x%08x) sector_count (0x%08x)\n",
			__func__, __LINE__, start_sector, 1);

		result = fwrite(buf, 1, NAND_FLASH_SECTOR_SIZE, fp);
		if (result < 0) {
			PRINTF("%s:%d: fwrite failed (0x%08x)\n", __func__, __LINE__, result);
			goto done;
		}

		start_sector += 1;
		sector_count -= 1;
	}

	fclose(fp);

	return 0;

done:

	if (fp)
		fclose(fp);

	result = lv2_storage_close(dev_handle);
	if (result)
		PRINTF("%s:%d: lv2_storage_close failed (0x%08x)\n", __func__, __LINE__, result);


	lv1_poke(ADDR_355, LV1_VALUE_HIDE);

	return result;

#undef NSECTORS
}

/*
 * dump_nor_flash
 */
int dump_nor_flash(void)
{
#define NSECTORS	16

	uint32_t dev_handle;
	struct storage_device_info info;
	FILE *fp;
	int start_sector, sector_count;
	uint32_t unknown2;
	uint8_t buf[NOR_FLASH_SECTOR_SIZE * NSECTORS];
	int result;

	dev_handle = 0;
	fp = NULL;

	result = lv2_storage_open(NOR_FLASH_DEV_ID, &dev_handle);
	if (result) {
		PRINTF("%s:%d: lv2_storage_open failed (0x%08x)\n", __func__, __LINE__, result);
		goto done;
	}

	fp = open_dump();
	if (!fp)
		goto done;

	result = lv2_storage_get_device_info(NOR_FLASH_DEV_ID, &info);
	if (result) {
		PRINTF("%s:%d: lv2_storage_get_device_info failed (0x%08x)\n", __func__, __LINE__, result);
		goto done;
	}

	PRINTF("%s:%d: capacity (0x%016llx)\n", __func__, __LINE__, info.capacity);

	start_sector = NOR_FLASH_START_SECTOR;
	sector_count = info.capacity;

	while (sector_count >= NSECTORS) {
		PRINTF("%s:%d: reading data start_sector (0x%08x) sector_count (0x%08x)\n",
			__func__, __LINE__, start_sector, NSECTORS);

		result = lv2_storage_read(dev_handle, 0, start_sector, NSECTORS, buf, &unknown2, NOR_FLASH_FLAGS);
		if (result) {
			PRINTF("%s:%d: lv2_storage_read failed (0x%08x)\n", __func__, __LINE__, result);
			goto done;
		}

		PRINTF("%s:%d: dumping data start_sector (0x%08x) sector_count (0x%08x)\n",
			__func__, __LINE__, start_sector, NSECTORS);

		result = fwrite(buf, 1, NSECTORS * NOR_FLASH_SECTOR_SIZE, fp);
		if (result < 0) {
			PRINTF("%s:%d: fwrite failed (0x%08x)\n", __func__, __LINE__, result);
			goto done;
		}

		start_sector += NSECTORS;
		sector_count -= NSECTORS;
	}

	while (sector_count) {
		PRINTF("%s:%d: reading data start_sector (0x%08x) sector_count (0x%08x)\n",
			__func__, __LINE__, start_sector, 1);

		result = lv2_storage_read(dev_handle, 0, start_sector, 1, buf, &unknown2, NOR_FLASH_FLAGS);
		if (result) {
			PRINTF("%s:%d: lv2_storage_read failed (0x%08x)\n", __func__, __LINE__, result);
			goto done;
		}

		PRINTF("%s:%d: dumping data start_sector (0x%08x) sector_count (0x%08x)\n",
			__func__, __LINE__, start_sector, 1);

		result = fwrite(buf, 1, NOR_FLASH_SECTOR_SIZE, fp);
		if (result < 0) {
			PRINTF("%s:%d: fwrite failed (0x%08x)\n", __func__, __LINE__, result);
			goto done;
		}

		start_sector += 1;
		sector_count -= 1;
	}

	fclose(fp);

	return 0;

done:

	if (fp)
		fclose(fp);

	result = lv2_storage_close(dev_handle);
	if (result)
		PRINTF("%s:%d: lv2_storage_close failed (0x%08x)\n", __func__, __LINE__, result);

	return result;

#undef NSECTORS
}

/*
 * main
 */
int main(int argc, char **argv)
{
	int vflash_on, result;

	netInitialize();

	udp_printf_init();

	PRINTF("%s:%d: start\n", __func__, __LINE__);

	vflash_on = is_vflash_on();

	PRINTF("%s:%d: vflash %s\n", __func__, __LINE__, vflash_on ? "on" : "off");

	if (vflash_on) {
		result = dump_nor_flash();
		if (result) {
			PRINTF("%s:%d: dump_nor_flash failed (0x%08x)\n", __func__, __LINE__, result);
			goto done;
		}
	} else {
		result = dump_nand_flash();
		if (result) {
			PRINTF("%s:%d: dump_nand_flash failed (0x%08x)\n", __func__, __LINE__, result);
			goto done;
		}
	}

	PRINTF("%s:%d: end\n", __func__, __LINE__);

	lv2_sm_ring_buzzer(0x1004, 0xa, 0x1b6);

done:

	udp_printf_deinit();

	netDeinitialize();

	return 0;
}
