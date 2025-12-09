/*
 * Copyright (c) 2024 XFS Modernization Project
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write the Free Software Foundation,
 * Inc.,  51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <xfs.h>

/*
 * CRC32C implementation for XFS V5 metadata checksums.
 *
 * XFS uses CRC32C (Castagnoli) with polynomial 0x1EDC6F41.
 * This is the same algorithm used by iSCSI, SCTP, and many storage protocols.
 *
 * The lookup table approach provides good performance while remaining portable.
 */

/*
 * CRC32C lookup table (Castagnoli polynomial 0x1EDC6F41, reflected)
 * Generated using the standard bit-at-a-time method.
 */
static const __uint32_t crc32c_table[256] = {
	0x00000000, 0xF26B8303, 0xE13B70F7, 0x1350F3F4,
	0xC79A971F, 0x35F1141C, 0x26A1E7E8, 0xD4CA64EB,
	0x8AD958CF, 0x78B2DBCC, 0x6BE22838, 0x9989AB3B,
	0x4D43CFD0, 0xBF284CD3, 0xAC78BF27, 0x5E133C24,
	0x105EC76F, 0xE235446C, 0xF165B798, 0x030E349B,
	0xD7C45070, 0x25AFD373, 0x36FF2087, 0xC494A384,
	0x9A879FA0, 0x68EC1CA3, 0x7BBCEF57, 0x89D76C54,
	0x5D1D08BF, 0xAF768BBC, 0xBC267848, 0x4E4DFB4B,
	0x20BD8EDE, 0xD2D60DDD, 0xC186FE29, 0x33ED7D2A,
	0xE727199F, 0x154C9A9C, 0x061C6968, 0xF477EA6B,
	0xAA64D64F, 0x580F554C, 0x4B5FA6B8, 0xB93425BB,
	0x6DFE4150, 0x9F95C253, 0x8CC531A7, 0x7EAEB2A4,
	0x30E349EF, 0xC288CAEC, 0xD1D83918, 0x23B3BA1B,
	0xF779DEF0, 0x05125DF3, 0x1642AE07, 0xE4292D04,
	0xBA3A1120, 0x48519223, 0x5B0161D7, 0xA96AE2D4,
	0x7DA0863F, 0x8FCB053C, 0x9C9BF6C8, 0x6EF075CB,
	0x417B1DBC, 0xB3109EBF, 0xA0406D4B, 0x522BEE48,
	0x86E18AA3, 0x748A09A0, 0x67DAFA54, 0x95B17957,
	0xCBA24573, 0x39C9C670, 0x2A993584, 0xD8F2B687,
	0x0C38D26C, 0xFE53516F, 0xED03A29B, 0x1F682198,
	0x5125DAD3, 0xA34E59D0, 0xB01EAA24, 0x42752927,
	0x96BF4DCC, 0x64D4CECF, 0x77843D3B, 0x85EFBE38,
	0xDBFC821C, 0x2997011F, 0x3AC7F2EB, 0xC8AC71E8,
	0x1C661503, 0xEE0D9600, 0xFD5D65F4, 0x0F36E6F7,
	0x61C69362, 0x93AD1061, 0x80FDE395, 0x72966096,
	0xA65C047D, 0x5437877E, 0x4767748A, 0xB50CF789,
	0xEB1FCBAD, 0x197448AE, 0x0A24BB5A, 0xF84F3859,
	0x2C855CB2, 0xDEEEDFB1, 0xCDBE2C45, 0x3FD5AF46,
	0x7198540D, 0x83F3D70E, 0x90A324FA, 0x62C8A7F9,
	0xB602C312, 0x44694011, 0x5739B3E5, 0xA55230E6,
	0xFB410CC2, 0x092A8FC1, 0x1A7A7C35, 0xE811FF36,
	0x3CDB9BDD, 0xCEB018DE, 0xDDE0EB2A, 0x2F8B6829,
	0x82F63B78, 0x709DB87B, 0x63CD4B8F, 0x91A6C88C,
	0x456CAC67, 0xB7072F64, 0xA457DC90, 0x563C5F93,
	0x082F63B7, 0xFA44E0B4, 0xE9141340, 0x1B7F9043,
	0xCFB5F4A8, 0x3DDE77AB, 0x2E8E845F, 0xDCE5075C,
	0x92A8FC17, 0x60C37F14, 0x73938CE0, 0x81F80FE3,
	0x55326B08, 0xA759E80B, 0xB4091BFF, 0x466298FC,
	0x1871A4D8, 0xEA1A27DB, 0xF94AD42F, 0x0B21572C,
	0xDFEB33C7, 0x2D80B0C4, 0x3ED04330, 0xCCBBC033,
	0xA24BB5A6, 0x502036A5, 0x4370C551, 0xB11B4652,
	0x65D122B9, 0x97BAA1BA, 0x84EA524E, 0x7681D14D,
	0x2892ED69, 0xDAF96E6A, 0xC9A99D9E, 0x3BC21E9D,
	0xEF087A76, 0x1D63F975, 0x0E330A81, 0xFC588982,
	0xB21572C9, 0x407EF1CA, 0x532E023E, 0xA145813D,
	0x758FE5D6, 0x87E466D5, 0x94B49521, 0x66DF1622,
	0x38CC2A06, 0xCAA7A905, 0xD9F75AF1, 0x2B9CD9F2,
	0xFF56BD19, 0x0D3D3E1A, 0x1E6DCDEE, 0xEC064EED,
	0xC38D26C4, 0x31E6A5C7, 0x22B65633, 0xD0DDD530,
	0x0417B1DB, 0xF67C32D8, 0xE52CC12C, 0x1747422F,
	0x49547E0B, 0xBB3FFD08, 0xA86F0EFC, 0x5A048DFF,
	0x8ECEE914, 0x7CA56A17, 0x6FF599E3, 0x9D9E1AE0,
	0xD3D3E1AB, 0x21B862A8, 0x32E8915C, 0xC083125F,
	0x144976B4, 0xE622F5B7, 0xF5720643, 0x07198540,
	0x590AB964, 0xAB613A67, 0xB831C993, 0x4A5A4A90,
	0x9E902E7B, 0x6CFBAD78, 0x7FAB5E8C, 0x8DC0DD8F,
	0xE330A81A, 0x115B2B19, 0x020BD8ED, 0xF0605BEE,
	0x24AA3F05, 0xD6C1BC06, 0xC5914FF2, 0x37FACCF1,
	0x69E9F0D5, 0x9B8273D6, 0x88D28022, 0x7AB90321,
	0xAE7367CA, 0x5C18E4C9, 0x4F48173D, 0xBD23943E,
	0xF36E6F75, 0x0105EC76, 0x12551F82, 0xE03E9C81,
	0x34F4F86A, 0xC69F7B69, 0xD5CF889D, 0x27A40B9E,
	0x79B737BA, 0x8BDCB4B9, 0x988C474D, 0x6AE7C44E,
	0xBE2DA0A5, 0x4C4623A6, 0x5F16D052, 0xAD7D5351
};

/*
 * Calculate CRC32C for a data buffer.
 *
 * @crc: initial CRC value (use XFS_CRC_SEED for starting value)
 * @buffer: pointer to the data buffer
 * @length: number of bytes to process
 *
 * Returns: updated CRC32C value
 */
__uint32_t
xfs_crc32c(
	__uint32_t	crc,
	const void	*buffer,
	size_t		length)
{
	const __uint8_t *data = (const __uint8_t *)buffer;

	while (length--) {
		crc = crc32c_table[(crc ^ *data++) & 0xFF] ^ (crc >> 8);
	}

	return crc;
}

/*
 * Calculate the intermediate checksum for a buffer, skipping the
 * checksum field itself.
 *
 * XFS stores the CRC inside the metadata block, so we need to skip
 * those 4 bytes during calculation. We do this by calculating the
 * CRC in two parts: before the checksum field and after it.
 *
 * @buffer: pointer to the data buffer
 * @length: total length of the data in bytes
 * @cksum_offset: byte offset of the checksum field to skip
 *
 * Returns: intermediate CRC32C value (call xfs_end_cksum to finalize)
 */
__uint32_t
xfs_start_cksum(
	char		*buffer,
	size_t		length,
	unsigned long	cksum_offset)
{
	__uint32_t crc;

	/* Calculate CRC up to the checksum field */
	crc = xfs_crc32c(XFS_CRC_SEED, buffer, cksum_offset);

	/* Skip the 4-byte checksum field and continue */
	crc = xfs_crc32c(crc, buffer + cksum_offset + sizeof(__uint32_t),
			 length - cksum_offset - sizeof(__uint32_t));

	return crc;
}

/*
 * Verify the checksum of a buffer against its stored CRC value.
 *
 * @buffer: pointer to the data buffer
 * @length: total length of the data in bytes
 * @cksum_offset: byte offset of the checksum field
 *
 * Returns: 1 if checksum is valid, 0 if invalid
 */
int
xfs_verify_cksum(
	char		*buffer,
	size_t		length,
	unsigned long	cksum_offset)
{
	__uint32_t crc;
	__uint32_t stored_crc;

	/* Calculate the expected checksum */
	crc = xfs_start_cksum(buffer, length, cksum_offset);
	crc = ~crc;  /* Finalize */

	/* Get the stored checksum (big-endian on disk) */
	stored_crc = be32_to_cpu(*(__be32 *)(buffer + cksum_offset));

	return crc == stored_crc;
}

/*
 * Update a buffer's checksum field with a newly calculated value.
 *
 * @buffer: pointer to the data buffer
 * @length: total length of the data in bytes
 * @cksum_offset: byte offset of the checksum field
 */
void
xfs_update_cksum(
	char		*buffer,
	size_t		length,
	unsigned long	cksum_offset)
{
	__uint32_t crc;

	/* Calculate the checksum */
	crc = xfs_start_cksum(buffer, length, cksum_offset);
	crc = ~crc;  /* Finalize */

	/* Store as big-endian */
	*(__be32 *)(buffer + cksum_offset) = cpu_to_be32(crc);
}