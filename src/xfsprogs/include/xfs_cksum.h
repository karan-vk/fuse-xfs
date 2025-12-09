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
#ifndef __XFS_CKSUM_H__
#define __XFS_CKSUM_H__

/*
 * CRC32C support for XFS V5 superblock format.
 *
 * XFS uses the CRC32C variant of CRC32, which is used in iSCSI and SCTP.
 * The polynomial used is 0x1EDC6F41 (Castagnoli).
 */

#define XFS_CRC_SEED	(~(__uint32_t)0)

/*
 * Calculate the intermediate checksum for a buffer.
 * The cksum_offset parameter specifies the offset in the buffer
 * where the checksum field is located (to skip it during calculation).
 *
 * @buffer: pointer to the data buffer
 * @length: length of the data in bytes
 * @cksum_offset: byte offset of the checksum field to skip
 *
 * Returns: intermediate CRC32C value (not yet finalized)
 */
extern __uint32_t xfs_start_cksum(char *buffer, size_t length,
				  unsigned long cksum_offset);

/*
 * Finalize a CRC32C checksum value.
 * This inverts the bits to produce the final checksum.
 *
 * @crc: intermediate CRC value from xfs_start_cksum
 *
 * Returns: finalized CRC32C checksum
 */
static inline __uint32_t xfs_end_cksum(__uint32_t crc)
{
	return ~crc;
}

/*
 * Calculate and verify the checksum for a buffer.
 * Computes the CRC32C checksum and compares it against the stored value.
 *
 * @buffer: pointer to the data buffer
 * @length: length of the data in bytes  
 * @cksum_offset: byte offset of the checksum field
 *
 * Returns: 1 if checksum is valid, 0 if invalid
 */
extern int xfs_verify_cksum(char *buffer, size_t length,
			    unsigned long cksum_offset);

/*
 * Update a buffer's checksum field with a newly calculated value.
 *
 * @buffer: pointer to the data buffer
 * @length: length of the data in bytes
 * @cksum_offset: byte offset of the checksum field
 */
extern void xfs_update_cksum(char *buffer, size_t length,
			     unsigned long cksum_offset);

/*
 * Raw CRC32C calculation function.
 * Calculates CRC32C over a buffer without any XFS-specific handling.
 *
 * @crc: initial/seed CRC value (use XFS_CRC_SEED for new calculation)
 * @buffer: pointer to the data buffer
 * @length: length of the data in bytes
 *
 * Returns: CRC32C value
 */
extern __uint32_t xfs_crc32c(__uint32_t crc, const void *buffer, size_t length);

/*
 * Superblock CRC offset for xfs_verify_cksum/xfs_update_cksum
 */
#define XFS_SB_CRC_OFF	offsetof(xfs_dsb_t, sb_crc)

/*
 * Calculate superblock checksum
 */
static inline __uint32_t xfs_sb_cksum(xfs_dsb_t *dsb)
{
	__uint32_t crc;

	crc = xfs_start_cksum((char *)dsb, sizeof(*dsb), XFS_SB_CRC_OFF);
	return xfs_end_cksum(crc);
}

/*
 * Verify superblock checksum
 */
static inline int xfs_sb_verify_cksum(xfs_dsb_t *dsb)
{
	return xfs_verify_cksum((char *)dsb, sizeof(*dsb), XFS_SB_CRC_OFF);
}

#endif /* __XFS_CKSUM_H__ */