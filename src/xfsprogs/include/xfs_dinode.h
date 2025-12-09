/*
 * Copyright (c) 2000,2002,2005 Silicon Graphics, Inc.
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
#ifndef __XFS_DINODE_H__
#define	__XFS_DINODE_H__

struct xfs_buf;
struct xfs_mount;

#define	XFS_DINODE_VERSION_1	1
#define	XFS_DINODE_VERSION_2	2
#define	XFS_DINODE_VERSION_3	3	/* V5 filesystem inodes */
#define XFS_DINODE_GOOD_VERSION(v)	\
	(((v) == XFS_DINODE_VERSION_1 || (v) == XFS_DINODE_VERSION_2 || \
	  (v) == XFS_DINODE_VERSION_3))
#define	XFS_DINODE_MAGIC	0x494e	/* 'IN' */

/*
 * Disk inode structure.
 * This is just the header; the inode is expanded to fill a variable size
 * with the last field expanding.  It is split into the core and "other"
 * because we only need the core part in the in-core inode.
 */
typedef struct xfs_timestamp {
	__be32		t_sec;		/* timestamp seconds */
	__be32		t_nsec;		/* timestamp nanoseconds */
} xfs_timestamp_t;

/*
 * V5 filesystem (inode version 3) inode timestamp format.
 * The timestamp is a 64-bit nanosecond counter, with the
 * bigtime feature providing timestamps from the year 0 to 2486.
 */
typedef struct xfs_timestamp_v3 {
	__be64		t_nsec;		/* nanoseconds since epoch */
} xfs_timestamp_v3_t;

/*
 * Note: Coordinate changes to this structure with the XFS_DI_* #defines
 * below, the offsets table in xfs_ialloc_log_di() and struct xfs_icdinode
 * in xfs_inode.h.
 *
 * For V3 inodes (version 5 filesystems), the dinode core has additional
 * fields after di_gen for V3-specific metadata.
 */
typedef struct xfs_dinode_core {
	__be16		di_magic;	/* inode magic # = XFS_DINODE_MAGIC */
	__be16		di_mode;	/* mode and type of file */
	__u8		di_version;	/* inode version */
	__u8		di_format;	/* format of di_c data */
	__be16		di_onlink;	/* old number of links to file */
	__be32		di_uid;		/* owner's user id */
	__be32		di_gid;		/* owner's group id */
	__be32		di_nlink;	/* number of links to file */
	__be16		di_projid_lo;	/* lower part of owner's project id */
	__be16		di_projid_hi;	/* higher part owner's project id */
	__u8		di_pad[6];	/* unused, zeroed space */
	__be16		di_flushiter;	/* incremented on flush */
	xfs_timestamp_t	di_atime;	/* time last accessed */
	xfs_timestamp_t	di_mtime;	/* time last modified */
	xfs_timestamp_t	di_ctime;	/* time created/inode modified */
	__be64		di_size;	/* number of bytes in file */
	__be64		di_nblocks;	/* # of direct & btree blocks used */
	__be32		di_extsize;	/* basic/minimum extent size for file */
	__be32		di_nextents;	/* number of extents in data fork */
	__be16		di_anextents;	/* number of extents in attribute fork*/
	__u8		di_forkoff;	/* attr fork offs, <<3 for 64b align */
	__s8		di_aformat;	/* format of attr fork's data */
	__be32		di_dmevmask;	/* DMIG event mask */
	__be16		di_dmstate;	/* DMIG state info */
	__be16		di_flags;	/* random flags, XFS_DIFLAG_... */
	__be32		di_gen;		/* generation number */

	/* start of V3 inode fields (XFS_DINODE_VERSION_3) */
	__be64		di_changecount;	/* V3: number of attribute changes */
	__be64		di_lsn;		/* V3: flush sequence */
	__be64		di_flags2;	/* V3: more random flags */
	__be32		di_cowextsize;	/* V3: CoW extent size hint */
	__u8		di_pad2[12];	/* V3: more padding */
	xfs_timestamp_t	di_crtime;	/* V3: creation time */
	__be64		di_ino;		/* V3: inode number */
	uuid_t		di_uuid;	/* V3: UUID */
} xfs_dinode_core_t;

/*
 * V3 inode CRC is located at a fixed offset after the core
 */
#define XFS_DINODE_CRC_OFF	offsetof(xfs_dinode_t, di_crc)

#define DI_MAX_FLUSH 0xffff

/*
 * V1/V2 disk inode structure (pre-V5 filesystem)
 */
typedef struct xfs_dinode
{
	xfs_dinode_core_t	di_core;
	/*
	 * In adding anything between the core and the union, be
	 * sure to update the macros like XFS_LITINO below.
	 */
	__be32			di_next_unlinked;/* agi unlinked list ptr */

	/*
	 * V3 inode CRC field - only valid if di_version == 3
	 * This is placed before the union so the CRC can be calculated
	 * over the entire inode core.
	 */
	__be32			di_crc;		/* V3: inode CRC32C */

	union {
		xfs_bmdr_block_t di_bmbt;	/* btree root block */
		xfs_bmbt_rec_32_t di_bmx[1];	/* extent list */
		xfs_dir2_sf_t	di_dir2sf;	/* shortform directory v2 */
		char		di_c[1];	/* local contents */
		__be32		di_dev;		/* device for S_IFCHR/S_IFBLK */
		uuid_t		di_muuid;	/* mount point value */
		char		di_symlink[1];	/* local symbolic link */
	}		di_u;
	union {
		xfs_bmdr_block_t di_abmbt;	/* btree root block */
		xfs_bmbt_rec_32_t di_abmx[1];	/* extent list */
		xfs_attr_shortform_t di_attrsf;	/* shortform attribute list */
	}		di_a;
} xfs_dinode_t;

/*
 * V3 inode size information
 * V3 inodes have a larger core due to the additional fields.
 */
#define XFS_DINODE_SIZE_V2	96	/* Size of V1/V2 inode core */
#define XFS_DINODE_SIZE_V3	176	/* Size of V3 inode core */

/*
 * Return the size of the inode core based on version
 */
static inline int xfs_dinode_size(int version)
{
	if (version == XFS_DINODE_VERSION_3)
		return XFS_DINODE_SIZE_V3;
	return XFS_DINODE_SIZE_V2;
}

/*
 * The 32 bit link count in the inode theoretically maxes out at UINT_MAX.
 * Since the pathconf interface is signed, we use 2^31 - 1 instead.
 * The old inode format had a 16 bit link count, so its maximum is USHRT_MAX.
 */
#define	XFS_MAXLINK		((1U << 31) - 1U)
#define	XFS_MAXLINK_1		65535U

/*
 * Bit names for logging disk inodes only
 */
#define	XFS_DI_MAGIC		0x0000001
#define	XFS_DI_MODE		0x0000002
#define	XFS_DI_VERSION		0x0000004
#define	XFS_DI_FORMAT		0x0000008
#define	XFS_DI_ONLINK		0x0000010
#define	XFS_DI_UID		0x0000020
#define	XFS_DI_GID		0x0000040
#define	XFS_DI_NLINK		0x0000080
#define	XFS_DI_PROJID		0x0000100
#define	XFS_DI_PAD		0x0000200
#define	XFS_DI_ATIME		0x0000400
#define	XFS_DI_MTIME		0x0000800
#define	XFS_DI_CTIME		0x0001000
#define	XFS_DI_SIZE		0x0002000
#define	XFS_DI_NBLOCKS		0x0004000
#define	XFS_DI_EXTSIZE		0x0008000
#define	XFS_DI_NEXTENTS		0x0010000
#define	XFS_DI_NAEXTENTS	0x0020000
#define	XFS_DI_FORKOFF		0x0040000
#define	XFS_DI_AFORMAT		0x0080000
#define	XFS_DI_DMEVMASK		0x0100000
#define	XFS_DI_DMSTATE		0x0200000
#define	XFS_DI_FLAGS		0x0400000
#define	XFS_DI_GEN		0x0800000
#define	XFS_DI_NEXT_UNLINKED	0x1000000
#define	XFS_DI_U		0x2000000
#define	XFS_DI_A		0x4000000
#define	XFS_DI_NUM_BITS		27
#define	XFS_DI_ALL_BITS		((1 << XFS_DI_NUM_BITS) - 1)
#define	XFS_DI_CORE_BITS	(XFS_DI_ALL_BITS & ~(XFS_DI_U|XFS_DI_A))

/*
 * Values for di_format
 */
typedef enum xfs_dinode_fmt
{
	XFS_DINODE_FMT_DEV,		/* CHR, BLK: di_dev */
	XFS_DINODE_FMT_LOCAL,		/* DIR, REG: di_c */
					/* LNK: di_symlink */
	XFS_DINODE_FMT_EXTENTS,		/* DIR, REG, LNK: di_bmx */
	XFS_DINODE_FMT_BTREE,		/* DIR, REG, LNK: di_bmbt */
	XFS_DINODE_FMT_UUID		/* MNT: di_uuid */
} xfs_dinode_fmt_t;

/*
 * Inode minimum and maximum sizes.
 */
#define	XFS_DINODE_MIN_LOG	8
#define	XFS_DINODE_MAX_LOG	11
#define	XFS_DINODE_MIN_SIZE	(1 << XFS_DINODE_MIN_LOG)
#define	XFS_DINODE_MAX_SIZE	(1 << XFS_DINODE_MAX_LOG)

/*
 * Inode size for given fs.
 */
/*
 * LITINO calculation for V3 inodes needs to account for larger core
 */
#define	XFS_LITINO(mp)	((mp)->m_litino)
#define	XFS_LITINO_V2(mp)	\
	((int)(((mp)->m_sb.sb_inodesize) - sizeof(xfs_dinode_core_t) - \
		sizeof(__be32)))  /* di_next_unlinked */
#define	XFS_LITINO_V3(mp)	\
	((int)(((mp)->m_sb.sb_inodesize) - sizeof(xfs_dinode_core_t) - \
		sizeof(__be32) - sizeof(__be32)))  /* di_next_unlinked + di_crc */

#define	XFS_BROOT_SIZE_ADJ	\
	(XFS_BTREE_LBLOCK_LEN - sizeof(xfs_bmdr_block_t))

/*
 * Inode data & attribute fork sizes, per inode.
 */
#define XFS_DFORK_Q(dip)		((dip)->di_core.di_forkoff != 0)
#define XFS_DFORK_BOFF(dip)		((int)((dip)->di_core.di_forkoff << 3))

#define XFS_DFORK_DSIZE(dip,mp) \
	(XFS_DFORK_Q(dip) ? \
		XFS_DFORK_BOFF(dip) : \
		XFS_LITINO(mp))
#define XFS_DFORK_ASIZE(dip,mp) \
	(XFS_DFORK_Q(dip) ? \
		XFS_LITINO(mp) - XFS_DFORK_BOFF(dip) : \
		0)
#define XFS_DFORK_SIZE(dip,mp,w) \
	((w) == XFS_DATA_FORK ? \
		XFS_DFORK_DSIZE(dip, mp) : \
		XFS_DFORK_ASIZE(dip, mp))

#define XFS_DFORK_DPTR(dip)		    ((dip)->di_u.di_c)
#define XFS_DFORK_APTR(dip)	\
	((dip)->di_u.di_c + XFS_DFORK_BOFF(dip))
#define XFS_DFORK_PTR(dip,w)	\
	((w) == XFS_DATA_FORK ? XFS_DFORK_DPTR(dip) : XFS_DFORK_APTR(dip))
#define XFS_DFORK_FORMAT(dip,w) \
	((w) == XFS_DATA_FORK ? \
		(dip)->di_core.di_format : \
		(dip)->di_core.di_aformat)
#define XFS_DFORK_NEXTENTS(dip,w) \
	((w) == XFS_DATA_FORK ? \
	 	be32_to_cpu((dip)->di_core.di_nextents) : \
	 	be16_to_cpu((dip)->di_core.di_anextents))

#define	XFS_BUF_TO_DINODE(bp)	((xfs_dinode_t *)XFS_BUF_PTR(bp))

/*
 * Values for di_flags
 * There should be a one-to-one correspondence between these flags and the
 * XFS_XFLAG_s.
 */
#define XFS_DIFLAG_REALTIME_BIT  0	/* file's blocks come from rt area */
#define XFS_DIFLAG_PREALLOC_BIT  1	/* file space has been preallocated */
#define XFS_DIFLAG_NEWRTBM_BIT   2	/* for rtbitmap inode, new format */
#define XFS_DIFLAG_IMMUTABLE_BIT 3	/* inode is immutable */
#define XFS_DIFLAG_APPEND_BIT    4	/* inode is append-only */
#define XFS_DIFLAG_SYNC_BIT      5	/* inode is written synchronously */
#define XFS_DIFLAG_NOATIME_BIT   6	/* do not update atime */
#define XFS_DIFLAG_NODUMP_BIT    7	/* do not dump */
#define XFS_DIFLAG_RTINHERIT_BIT 8	/* create with realtime bit set */
#define XFS_DIFLAG_PROJINHERIT_BIT   9	/* create with parents projid */
#define XFS_DIFLAG_NOSYMLINKS_BIT   10	/* disallow symlink creation */
#define XFS_DIFLAG_EXTSIZE_BIT      11	/* inode extent size allocator hint */
#define XFS_DIFLAG_EXTSZINHERIT_BIT 12	/* inherit inode extent size */
#define XFS_DIFLAG_NODEFRAG_BIT     13	/* do not reorganize/defragment */
#define XFS_DIFLAG_FILESTREAM_BIT   14  /* use filestream allocator */
#define XFS_DIFLAG_REALTIME      (1 << XFS_DIFLAG_REALTIME_BIT)
#define XFS_DIFLAG_PREALLOC      (1 << XFS_DIFLAG_PREALLOC_BIT)
#define XFS_DIFLAG_NEWRTBM       (1 << XFS_DIFLAG_NEWRTBM_BIT)
#define XFS_DIFLAG_IMMUTABLE     (1 << XFS_DIFLAG_IMMUTABLE_BIT)
#define XFS_DIFLAG_APPEND        (1 << XFS_DIFLAG_APPEND_BIT)
#define XFS_DIFLAG_SYNC          (1 << XFS_DIFLAG_SYNC_BIT)
#define XFS_DIFLAG_NOATIME       (1 << XFS_DIFLAG_NOATIME_BIT)
#define XFS_DIFLAG_NODUMP        (1 << XFS_DIFLAG_NODUMP_BIT)
#define XFS_DIFLAG_RTINHERIT     (1 << XFS_DIFLAG_RTINHERIT_BIT)
#define XFS_DIFLAG_PROJINHERIT   (1 << XFS_DIFLAG_PROJINHERIT_BIT)
#define XFS_DIFLAG_NOSYMLINKS    (1 << XFS_DIFLAG_NOSYMLINKS_BIT)
#define XFS_DIFLAG_EXTSIZE       (1 << XFS_DIFLAG_EXTSIZE_BIT)
#define XFS_DIFLAG_EXTSZINHERIT  (1 << XFS_DIFLAG_EXTSZINHERIT_BIT)
#define XFS_DIFLAG_NODEFRAG      (1 << XFS_DIFLAG_NODEFRAG_BIT)
#define XFS_DIFLAG_FILESTREAM    (1 << XFS_DIFLAG_FILESTREAM_BIT)

#ifdef CONFIG_XFS_RT
#define XFS_IS_REALTIME_INODE(ip) ((ip)->i_d.di_flags & XFS_DIFLAG_REALTIME)
#else
#define XFS_IS_REALTIME_INODE(ip) (0)
#endif

#define XFS_DIFLAG_ANY \
	(XFS_DIFLAG_REALTIME | XFS_DIFLAG_PREALLOC | XFS_DIFLAG_NEWRTBM | \
	 XFS_DIFLAG_IMMUTABLE | XFS_DIFLAG_APPEND | XFS_DIFLAG_SYNC | \
	 XFS_DIFLAG_NOATIME | XFS_DIFLAG_NODUMP | XFS_DIFLAG_RTINHERIT | \
	 XFS_DIFLAG_PROJINHERIT | XFS_DIFLAG_NOSYMLINKS | XFS_DIFLAG_EXTSIZE | \
	 XFS_DIFLAG_EXTSZINHERIT | XFS_DIFLAG_NODEFRAG | XFS_DIFLAG_FILESTREAM)

/*
 * Values for di_flags2 (V3 inodes only)
 */
#define XFS_DIFLAG2_DAX_BIT		0	/* use DAX for this inode */
#define XFS_DIFLAG2_REFLINK_BIT		1	/* file's blocks may be shared */
#define XFS_DIFLAG2_COWEXTSIZE_BIT	2	/* copy on write extent size hint */
#define XFS_DIFLAG2_BIGTIME_BIT		3	/* big timestamps */
#define XFS_DIFLAG2_NREXT64_BIT		4	/* 64-bit extent counter */

#define XFS_DIFLAG2_DAX		(1ULL << XFS_DIFLAG2_DAX_BIT)
#define XFS_DIFLAG2_REFLINK	(1ULL << XFS_DIFLAG2_REFLINK_BIT)
#define XFS_DIFLAG2_COWEXTSIZE	(1ULL << XFS_DIFLAG2_COWEXTSIZE_BIT)
#define XFS_DIFLAG2_BIGTIME	(1ULL << XFS_DIFLAG2_BIGTIME_BIT)
#define XFS_DIFLAG2_NREXT64	(1ULL << XFS_DIFLAG2_NREXT64_BIT)

#define XFS_DIFLAG2_ANY \
	(XFS_DIFLAG2_DAX | XFS_DIFLAG2_REFLINK | XFS_DIFLAG2_COWEXTSIZE | \
	 XFS_DIFLAG2_BIGTIME | XFS_DIFLAG2_NREXT64)

/*
 * Helper to check if an inode is V3 format
 */
static inline int xfs_dinode_is_v3(xfs_dinode_t *dip)
{
	return dip->di_core.di_version == XFS_DINODE_VERSION_3;
}

/*
 * Get the inode's UUID - for V3 inodes this is in di_uuid,
 * for older inodes use the superblock UUID
 */
static inline uuid_t *xfs_dinode_get_uuid(xfs_dinode_t *dip)
{
	if (xfs_dinode_is_v3(dip))
		return &dip->di_core.di_uuid;
	return NULL;  /* Caller should use superblock UUID */
}

/*
 * V3 inode CRC verification helpers
 * These are defined here but implemented in xfs_cksum.c
 */
#define XFS_DINODE_CRC_SIZE	\
	(offsetof(xfs_dinode_t, di_crc) + sizeof(__be32))

#endif	/* __XFS_DINODE_H__ */
