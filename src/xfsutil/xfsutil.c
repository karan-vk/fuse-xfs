#include <xfsutil.h>
#include <xfs/libxfs.h>
#include <sys/dirent.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>

#define do_log printf

#define PATH_SEP '/'

#define XFS_FORCED_SHUTDOWN(mp) 0

#define XFS_ERROR(code) (-code)

#define XFS_STATS_INC(stat)

/*
 * Convert XFS directory file type to POSIX DT_* type for readdir.
 * This is used when the filesystem has FTYPE support (V5 format).
 */
unsigned char
xfs_ftype_to_dtype(
	__uint8_t	ftype)
{
	switch (ftype) {
	case XFS_DIR3_FT_REG_FILE:
		return DT_REG;
	case XFS_DIR3_FT_DIR:
		return DT_DIR;
	case XFS_DIR3_FT_CHRDEV:
		return DT_CHR;
	case XFS_DIR3_FT_BLKDEV:
		return DT_BLK;
	case XFS_DIR3_FT_FIFO:
		return DT_FIFO;
	case XFS_DIR3_FT_SOCK:
		return DT_SOCK;
	case XFS_DIR3_FT_SYMLINK:
		return DT_LNK;
	case XFS_DIR3_FT_WHT:
		return DT_WHT;
	case XFS_DIR3_FT_UNKNOWN:
	default:
		return DT_UNKNOWN;
	}
}

/*
 * Check if the filesystem has FTYPE support (V5 or V4 with ftype flag)
 */
static inline int
xfs_has_ftype(
	xfs_mount_t	*mp)
{
	return xfs_sb_version_hasftype(&mp->m_sb);
}

/*
 * Get entry size based on filesystem version and FTYPE support
 */
static inline int
xfs_dir_entry_size(
	xfs_mount_t	*mp,
	int		namelen)
{
	if (xfs_has_ftype(mp))
		return xfs_dir3_data_entsize(namelen);
	return xfs_dir2_data_entsize(namelen);
}

/*
 * Get file type from directory entry (returns DT_UNKNOWN if not FTYPE-enabled)
 */
static unsigned char
xfs_dir_entry_ftype(
	xfs_mount_t		*mp,
	xfs_dir2_data_entry_t	*dep)
{
	if (xfs_has_ftype(mp)) {
		__uint8_t ftype = xfs_dir3_data_get_ftype(dep);
		return xfs_ftype_to_dtype(ftype);
	}
	return DT_UNKNOWN;
}

/*********** taken from xfs/xfs_dir2_leaf.c in the linux kernel ******/

/*
 * Getdents (readdir) for leaf and node directories.
 * This reads the data blocks only, so is the same for both forms.
 */
int						/* error */
xfs_dir2_leaf_getdents(
                       xfs_inode_t		*dp,		/* incore directory inode */
                       void			*dirent,
                       size_t			bufsize,
                       xfs_off_t		*offset,
                       filldir_t		filldir)
{
	xfs_dabuf_t		*bp;		/* data block buffer */
	int			byteoff;	/* offset in current block */
	xfs_dir2_db_t		curdb;		/* db for current block */
	xfs_dir2_off_t		curoff;		/* current overall offset */
	xfs_dir2_data_t		*data;		/* data block structure */
	xfs_dir2_data_entry_t	*dep;		/* data entry */
	xfs_dir2_data_unused_t	*dup;		/* unused entry */
	int			error = 0;	/* error return value */
	int			i;		/* temporary loop index */
	int			j;		/* temporary loop index */
	int			length;		/* temporary length value */
	xfs_bmbt_irec_t		*map;		/* map vector for blocks */
	xfs_extlen_t		map_blocks;	/* number of fsbs in map */
	xfs_dablk_t		map_off;	/* last mapped file offset */
	int			map_size;	/* total entries in *map */
	int			map_valid;	/* valid entries in *map */
	xfs_mount_t		*mp;		/* filesystem mount point */
	xfs_dir2_off_t		newoff;		/* new curoff after new blk */
	int			nmap;		/* mappings to ask xfs_bmapi */
	char			*ptr = NULL;	/* pointer to current data */
	int			ra_current;	/* number of read-ahead blks */
	int			ra_index;	/* *map index for read-ahead */
	int			ra_offset;	/* map entry offset for ra */
	int			ra_want;	/* readahead count wanted */
    
	/*
	 * If the offset is at or past the largest allowed value,
	 * give up right away.
	 */
	if (*offset >= XFS_DIR2_MAX_DATAPTR)
		return 0;
    
	mp = dp->i_mount;
    
	/*
	 * Set up to bmap a number of blocks based on the caller's
	 * buffer size, the directory block size, and the filesystem
	 * block size.
	 */
	map_size = howmany(bufsize + mp->m_dirblksize, mp->m_sb.sb_blocksize);
	map = kmem_alloc(map_size * sizeof(*map), KM_SLEEP);
	map_valid = ra_index = ra_offset = ra_current = map_blocks = 0;
	bp = NULL;
    
	/*
	 * Inside the loop we keep the main offset value as a byte offset
	 * in the directory file.
	 */
	curoff = xfs_dir2_dataptr_to_byte(mp, *offset);
    
	/*
	 * Force this conversion through db so we truncate the offset
	 * down to get the start of the data block.
	 */
	map_off = xfs_dir2_db_to_da(mp, xfs_dir2_byte_to_db(mp, curoff));
	/*
	 * Loop over directory entries until we reach the end offset.
	 * Get more blocks and readahead as necessary.
	 */
	while (curoff < XFS_DIR2_LEAF_OFFSET) {
		/*
		 * If we have no buffer, or we're off the end of the
		 * current buffer, need to get another one.
		 */
		if (!bp || ptr >= (char *)bp->data + mp->m_dirblksize) {
			/*
			 * If we have a buffer, we need to release it and
			 * take it out of the mapping.
			 */
			if (bp) {
				xfs_da_brelse(NULL, bp);
				bp = NULL;
				map_blocks -= mp->m_dirblkfsbs;
				/*
				 * Loop to get rid of the extents for the
				 * directory block.
				 */
				for (i = mp->m_dirblkfsbs; i > 0; ) {
					j = MIN((int)map->br_blockcount, i);
					map->br_blockcount -= j;
					map->br_startblock += j;
					map->br_startoff += j;
					/*
					 * If mapping is done, pitch it from
					 * the table.
					 */
					if (!map->br_blockcount && --map_valid)
						memmove(&map[0], &map[1],
                                sizeof(map[0]) *
                                map_valid);
					i -= j;
				}
			}
			/*
			 * Recalculate the readahead blocks wanted.
			 */
			ra_want = howmany(bufsize + mp->m_dirblksize,
                              mp->m_sb.sb_blocksize) - 1;
			ASSERT(ra_want >= 0);
            
			/*
			 * If we don't have as many as we want, and we haven't
			 * run out of data blocks, get some more mappings.
			 */
			if (1 + ra_want > map_blocks &&
			    map_off <
			    xfs_dir2_byte_to_da(mp, XFS_DIR2_LEAF_OFFSET)) {
				/*
				 * Get more bmaps, fill in after the ones
				 * we already have in the table.
				 */
				nmap = map_size - map_valid;
				error = xfs_bmapi(NULL, dp,
                                  map_off,
                                  xfs_dir2_byte_to_da(mp,
                                                      XFS_DIR2_LEAF_OFFSET) - map_off,
                                  XFS_BMAPI_METADATA, NULL, 0,
                                  &map[map_valid], &nmap, NULL, NULL);
				/*
				 * Don't know if we should ignore this or
				 * try to return an error.
				 * The trouble with returning errors
				 * is that readdir will just stop without
				 * actually passing the error through.
				 */
				if (error)
					break;	/* XXX */
				/*
				 * If we got all the mappings we asked for,
				 * set the final map offset based on the
				 * last bmap value received.
				 * Otherwise, we've reached the end.
				 */
				if (nmap == map_size - map_valid)
					map_off =
					map[map_valid + nmap - 1].br_startoff +
					map[map_valid + nmap - 1].br_blockcount;
				else
					map_off =
                    xfs_dir2_byte_to_da(mp,
                                        XFS_DIR2_LEAF_OFFSET);
				/*
				 * Look for holes in the mapping, and
				 * eliminate them.  Count up the valid blocks.
				 */
				for (i = map_valid; i < map_valid + nmap; ) {
					if (map[i].br_startblock ==
					    HOLESTARTBLOCK) {
						nmap--;
						length = map_valid + nmap - i;
						if (length)
							memmove(&map[i],
                                    &map[i + 1],
                                    sizeof(map[i]) *
                                    length);
					} else {
						map_blocks +=
                        map[i].br_blockcount;
						i++;
					}
				}
				map_valid += nmap;
			}
			/*
			 * No valid mappings, so no more data blocks.
			 */
			if (!map_valid) {
				curoff = xfs_dir2_da_to_byte(mp, map_off);
				break;
			}
			/*
			 * Read the directory block starting at the first
			 * mapping.
			 */
			curdb = xfs_dir2_da_to_db(mp, map->br_startoff);
			error = xfs_da_read_buf(NULL, dp, map->br_startoff,
                                    map->br_blockcount >= mp->m_dirblkfsbs ?
                                    XFS_FSB_TO_DADDR(mp, map->br_startblock) :
                                    -1,
                                    &bp, XFS_DATA_FORK);
			/*
			 * Should just skip over the data block instead
			 * of giving up.
			 */
			if (error)
				break;	/* XXX */
			/*
			 * Adjust the current amount of read-ahead: we just
			 * read a block that was previously ra.
			 */
			if (ra_current)
				ra_current -= mp->m_dirblkfsbs;
			/*
			 * Do we need more readahead?
			 */
			for (ra_index = ra_offset = i = 0;
			     ra_want > ra_current && i < map_blocks;
			     i += mp->m_dirblkfsbs) {
				ASSERT(ra_index < map_valid);
				/*
				 * Read-ahead a contiguous directory block.
				 */
				if (i > ra_current &&
				    map[ra_index].br_blockcount >=
				    mp->m_dirblkfsbs) {
                    //TODO: make sure this is right, should readahead be 
                    //replaced by readbuf?
					//xfs_buf_readahead(mp->m_ddev_targp,
                    //                  XFS_FSB_TO_DADDR(mp,
                    //                                   map[ra_index].br_startblock +
                    //                                   ra_offset),
                    //                  (int)BTOBB(mp->m_dirblksize));
                    //TODO: figure out flags, flags =0
					//libxfs_readbuf(mp->m_dev,
                    //               XFS_FSB_TO_DADDR(mp,
                    //                                map[ra_index].br_startblock +
                    //                                ra_offset),
                    //               (int)BTOBB(mp->m_dirblksize), 0);
					ra_current = i;
				}
				/*
				 * Read-ahead a non-contiguous directory block.
				 * This doesn't use our mapping, but this
				 * is a very rare case.
				 */
				else if (i > ra_current) {
					(void)xfs_da_reada_buf(NULL, dp,
                                           map[ra_index].br_startoff +
                                           ra_offset, XFS_DATA_FORK);
					ra_current = i;
				}
				/*
				 * Advance offset through the mapping table.
				 */
				for (j = 0; j < mp->m_dirblkfsbs; j++) {
					/*
					 * The rest of this extent but not
					 * more than a dir block.
					 */
					length = MIN(mp->m_dirblkfsbs,
                                 (int)(map[ra_index].br_blockcount -
                                       ra_offset));
					j += length;
					ra_offset += length;
					/*
					 * Advance to the next mapping if
					 * this one is used up.
					 */
					if (ra_offset ==
					    map[ra_index].br_blockcount) {
						ra_offset = 0;
						ra_index++;
					}
				}
			}
			/*
			 * Having done a read, we need to set a new offset.
			 */
			newoff = xfs_dir2_db_off_to_byte(mp, curdb, 0);
			/*
			 * Start of the current block.
			 */
			if (curoff < newoff)
				curoff = newoff;
			/*
			 * Make sure we're in the right block.
			 */
			else if (curoff > newoff)
				ASSERT(xfs_dir2_byte_to_db(mp, curoff) ==
				       curdb);
			data = bp->data;
			xfs_dir2_data_check(dp, bp);
			/*
			 * Find our position in the block.
			 */
			ptr = (char *)&data->u;
			byteoff = xfs_dir2_byte_to_off(mp, curoff);
			/*
			 * Skip past the header.
			 */
			if (byteoff == 0)
				curoff += (uint)sizeof(data->hdr);
			/*
			 * Skip past entries until we reach our offset.
			 */
			else {
				while ((char *)ptr - (char *)data < byteoff) {
					dup = (xfs_dir2_data_unused_t *)ptr;
                    
					if (be16_to_cpu(dup->freetag)
                        == XFS_DIR2_DATA_FREE_TAG) {
                        
						length = be16_to_cpu(dup->length);
						ptr += length;
						continue;
					}
					dep = (xfs_dir2_data_entry_t *)ptr;
						length = xfs_dir_entry_size(mp, dep->namelen);
						ptr += length;
				}
				/*
				 * Now set our real offset.
				 */
				curoff =
                xfs_dir2_db_off_to_byte(mp,
                                        xfs_dir2_byte_to_db(mp, curoff),
                                        (char *)ptr - (char *)data);
				if (ptr >= (char *)data + mp->m_dirblksize) {
					continue;
				}
			}
		}
		/*
		 * We have a pointer to an entry.
		 * Is it a live one?
		 */
		dup = (xfs_dir2_data_unused_t *)ptr;
		/*
		 * No, it's unused, skip over it.
		 */
		if (be16_to_cpu(dup->freetag) == XFS_DIR2_DATA_FREE_TAG) {
			length = be16_to_cpu(dup->length);
			ptr += length;
			curoff += length;
			continue;
		}
		      
		dep = (xfs_dir2_data_entry_t *)ptr;
		length = xfs_dir_entry_size(mp, dep->namelen);
		      
		/* Get file type for V5/FTYPE-enabled filesystems */
		unsigned char dtype = xfs_dir_entry_ftype(mp, dep);
		      
		if (filldir(dirent, (char *)dep->name, dep->namelen,
		                  xfs_dir2_byte_to_dataptr(mp, curoff) & 0x7fffffff,
		                  be64_to_cpu(dep->inumber), dtype))
			break;
        
		/*
		 * Advance to next entry in the block.
		 */
		ptr += length;
		curoff += length;
		/* bufsize may have just been a guess; don't go negative */
		bufsize = bufsize > length ? bufsize - length : 0;
	}
    
	/*
	 * All done.  Set output offset value to current offset.
	 */
	if (curoff > xfs_dir2_dataptr_to_byte(mp, XFS_DIR2_MAX_DATAPTR))
		*offset = XFS_DIR2_MAX_DATAPTR & 0x7fffffff;
	else
		*offset = xfs_dir2_byte_to_dataptr(mp, curoff) & 0x7fffffff;
	kmem_free(map);
	if (bp)
		xfs_da_brelse(NULL, bp);
	return error;
}

/*********** taken from xfs/xfs_dir2_sf.c in the linux kernel ******/

int						/* error */
xfs_dir2_sf_getdents(
                     xfs_inode_t		*dp,		/* incore directory inode */
                     void			*dirent,
                     xfs_off_t		*offset,
                     filldir_t		filldir)
{
	int			i;		/* shortform entry number */
	xfs_mount_t		*mp;		/* filesystem mount point */
	xfs_dir2_dataptr_t	off;		/* current entry's offset */
	xfs_dir2_sf_entry_t	*sfep;		/* shortform directory entry */
	xfs_dir2_sf_t		*sfp;		/* shortform structure */
	xfs_dir2_dataptr_t	dot_offset;
	xfs_dir2_dataptr_t	dotdot_offset;
	xfs_ino_t		ino;
    
	mp = dp->i_mount;
    
	ASSERT(dp->i_df.if_flags & XFS_IFINLINE);
	/*
	 * Give up if the directory is way too short.
	 */
	if (dp->i_d.di_size < offsetof(xfs_dir2_sf_hdr_t, parent)) {
        //TODO: how should shutdown be handled?
        ASSERT(XFS_FORCED_SHUTDOWN(mp));
		return XFS_ERROR(EIO);
	}
    
	ASSERT(dp->i_df.if_bytes == dp->i_d.di_size);
	ASSERT(dp->i_df.if_u1.if_data != NULL);
    
	sfp = (xfs_dir2_sf_t *)dp->i_df.if_u1.if_data;
    
	ASSERT(dp->i_d.di_size >= xfs_dir2_sf_hdr_size(sfp->hdr.i8count));
    
	/*
	 * If the block number in the offset is out of range, we're done.
	 */
	if (xfs_dir2_dataptr_to_db(mp, *offset) > mp->m_dirdatablk)
		return 0;
    
	/*
	 * Precalculate offsets for . and .. as we will always need them.
	 *
	 * XXX(hch): the second argument is sometimes 0 and sometimes
	 * mp->m_dirdatablk.
	 */
	dot_offset = xfs_dir2_db_off_to_dataptr(mp, mp->m_dirdatablk,
                                            XFS_DIR2_DATA_DOT_OFFSET);
	dotdot_offset = xfs_dir2_db_off_to_dataptr(mp, mp->m_dirdatablk,
                                               XFS_DIR2_DATA_DOTDOT_OFFSET);
    
	/*
	 * Put . entry unless we're starting past it.
	 */
	if (*offset <= dot_offset) {
		if (filldir(dirent, ".", 1, dot_offset & 0x7fffffff, dp->i_ino, DT_DIR)) {
			*offset = dot_offset & 0x7fffffff;
			return 0;
		}
	}
    
	/*
	 * Put .. entry unless we're starting past it.
	 */
	if (*offset <= dotdot_offset) {
		ino = xfs_dir2_sf_get_inumber(sfp, &sfp->hdr.parent);
		if (filldir(dirent, "..", 2, dotdot_offset & 0x7fffffff, ino, DT_DIR)) {
			*offset = dotdot_offset & 0x7fffffff;
			return 0;
		}
	}
    
	/*
	 * Check if this filesystem has FTYPE support
	 */
	int has_ftype = xfs_has_ftype(mp);

	/*
	 * Loop while there are more entries and put'ing works.
	 */
	sfep = xfs_dir2_sf_firstentry(sfp);
	for (i = 0; i < sfp->hdr.count; i++) {
		off = xfs_dir2_db_off_to_dataptr(mp, mp->m_dirdatablk,
	                                        xfs_dir2_sf_get_offset(sfep));
	       
		if (*offset > off) {
			sfep = has_ftype ?
				(xfs_dir2_sf_entry_t *)((char *)sfep + xfs_dir3_sf_entsize_byentry(sfp, sfep)) :
				xfs_dir2_sf_nextentry(sfp, sfep);
			continue;
		}
	       
		/* Get inode number - account for ftype byte if present */
		ino = has_ftype ?
			xfs_dir2_sf_get_inumber(sfp, xfs_dir3_sf_inumberp(sfep)) :
			xfs_dir2_sf_get_inumber(sfp, xfs_dir2_sf_inumberp(sfep));
		
		/* Get file type for FTYPE-enabled filesystems */
		unsigned char dtype = DT_UNKNOWN;
		if (has_ftype) {
			__uint8_t ftype = xfs_dir3_sf_get_ftype(sfep);
			dtype = xfs_ftype_to_dtype(ftype);
		}
		
		if (filldir(dirent, (char *)sfep->name, sfep->namelen,
	                   off & 0x7fffffff, ino, dtype)) {
			*offset = off & 0x7fffffff;
			return 0;
		}
		sfep = has_ftype ?
			(xfs_dir2_sf_entry_t *)((char *)sfep + xfs_dir3_sf_entsize_byentry(sfp, sfep)) :
			xfs_dir2_sf_nextentry(sfp, sfep);
	}
    
	*offset = xfs_dir2_db_off_to_dataptr(mp, mp->m_dirdatablk + 1, 0) &
    0x7fffffff;
	return 0;
}

/*********** taken from xfs/xfs_dir2_block.c in the linux kernel ******/
/*
 * Readdir for block directories.
 */
int						/* error */
xfs_dir2_block_getdents(
                        xfs_inode_t		*dp,		/* incore inode */
                        void			*dirent,
                        xfs_off_t		*offset,
                        filldir_t		filldir)
{
	xfs_dir2_block_t	*block;		/* directory block structure */
	xfs_dabuf_t		*bp;		/* buffer for block */
	xfs_dir2_block_tail_t	*btp;		/* block tail */
	xfs_dir2_data_entry_t	*dep;		/* block data entry */
	xfs_dir2_data_unused_t	*dup;		/* block unused entry */
	char			*endptr;	/* end of the data entries */
	int			error;		/* error return value */
	xfs_mount_t		*mp;		/* filesystem mount point */
	char			*ptr;		/* current data entry */
	int			wantoff;	/* starting block offset */
	xfs_off_t		cook;
    
	mp = dp->i_mount;
	/*
	 * If the block number in the offset is out of range, we're done.
	 */
	if (xfs_dir2_dataptr_to_db(mp, *offset) > mp->m_dirdatablk) {
		return 0;
	}
	/*
	 * Can't read the block, give up, else get dabuf in bp.
	 */
	error = xfs_da_read_buf(NULL, dp, mp->m_dirdatablk, -1,
                            &bp, XFS_DATA_FORK);
	if (error)
		return error;
    
	ASSERT(bp != NULL);
	/*
	 * Extract the byte offset we start at from the seek pointer.
	 * We'll skip entries before this.
	 */
	wantoff = xfs_dir2_dataptr_to_off(mp, *offset);
	block = bp->data;
	xfs_dir2_data_check(dp, bp);
	/*
	 * Set up values for the loop.
	 */
	btp = xfs_dir2_block_tail_p(mp, block);
	ptr = (char *)block->u;
	endptr = (char *)xfs_dir2_block_leaf_p(btp);
    
	/*
	 * Loop over the data portion of the block.
	 * Each object is a real entry (dep) or an unused one (dup).
	 */
	while (ptr < endptr) {
		dup = (xfs_dir2_data_unused_t *)ptr;
		/*
		 * Unused, skip it.
		 */
		if (be16_to_cpu(dup->freetag) == XFS_DIR2_DATA_FREE_TAG) {
			ptr += be16_to_cpu(dup->length);
			continue;
		}
        
		dep = (xfs_dir2_data_entry_t *)ptr;
		      
		/*
		 * Bump pointer for the next iteration.
		 */
		ptr += xfs_dir_entry_size(mp, dep->namelen);
		/*
		 * The entry is before the desired starting point, skip it.
		 */
		if ((char *)dep - (char *)block < wantoff)
			continue;
		      
		cook = xfs_dir2_db_off_to_dataptr(mp, mp->m_dirdatablk,
		                                        (char *)dep - (char *)block);
		      
		/* Get file type for V5/FTYPE-enabled filesystems */
		unsigned char dtype = xfs_dir_entry_ftype(mp, dep);
		      
		/*
		 * If it didn't fit, set the final offset to here & return.
		 */
		if (filldir(dirent, (char *)dep->name, dep->namelen,
		                  cook & 0x7fffffff, be64_to_cpu(dep->inumber),
		                  dtype)) {
			*offset = cook & 0x7fffffff;
			xfs_da_brelse(NULL, bp);
			return 0;
		}
	}
    
	/*
	 * Reached the end of the block.
	 * Set the offset to a non-existent block 1 and return.
	 */
	*offset = xfs_dir2_db_off_to_dataptr(mp, mp->m_dirdatablk + 1, 0) &
    0x7fffffff;
	xfs_da_brelse(NULL, bp);
	return 0;
}

/*********** taken from xfs/xfs_dir2.c in the linux kernel ************/
/*
 * Read a directory.
 */
int
xfs_readdir(
            xfs_inode_t	*dp,
            void		*dirent,
            size_t		bufsize,
            xfs_off_t	*offset,
            filldir_t	filldir)
{
	int		rval;		/* return value */
	int		v;		/* type-checking value */
    
    if (!(dp->i_d.di_mode & S_IFDIR))
		return XFS_ERROR(ENOTDIR);
    
    //TODO: find suitable replacement
	//trace_xfs_readdir(dp);
    
    //TODO: handle shutdown?
	if (XFS_FORCED_SHUTDOWN(dp->i_mount))
		return XFS_ERROR(EIO);
    
	ASSERT((dp->i_d.di_mode & S_IFMT) == S_IFDIR);
    //TODO: do we need this?
	XFS_STATS_INC(xs_dir_getdents);
    
	if (dp->i_d.di_format == XFS_DINODE_FMT_LOCAL)
		rval = xfs_dir2_sf_getdents(dp, dirent, offset, filldir);
	else if ((rval = xfs_dir2_isblock(NULL, dp, &v)))
		;
	else if (v)
		rval = xfs_dir2_block_getdents(dp, dirent, offset, filldir);
	else
		rval = xfs_dir2_leaf_getdents(dp, dirent, bufsize, offset,
                                      filldir);
	return rval;
}
/**********************************************************************/

int min(int x, int y) {
    if (x < y) return x;
    else return y;
}

int copy_extent_to_buffer(xfs_mount_t *mp, xfs_bmbt_irec_t rec, void *buffer, off_t offset, size_t len) {
    xfs_buf_t *block_buffer;
    int64_t copylen, copy_start;
    xfs_daddr_t block, start, end;
    char *src;
    off_t cofs = offset;
    
    xfs_off_t block_start;
    xfs_daddr_t block_size = XFS_FSB_TO_B(mp, 1);
    //xfs_daddr_t extent_size = XFS_FSB_TO_B(mp, rec.br_blockcount);
    xfs_daddr_t extent_start = XFS_FSB_TO_B(mp, rec.br_startoff);

    /* compute a block to start reading from */
    if (offset >= extent_start) {
        start = XFS_B_TO_FSBT(mp, offset - extent_start);
    } else {
        buffer = buffer + extent_start - offset;
        cofs += extent_start - offset;
        start = 0;
    }

    end = min(rec.br_blockcount, XFS_B_TO_FSBT(mp, offset + len - extent_start - 1) + 1);

    for (block=start; block<end; block++) {
        block_start = XFS_FSB_TO_B(mp, (rec.br_startoff + block));        
        block_buffer = libxfs_readbuf(mp->m_dev, XFS_FSB_TO_DADDR(mp, (rec.br_startblock + block)), 
                                      XFS_FSB_TO_BB(mp, 1), 0);
        if (block_buffer == NULL) {
            printf("Buffer error\n");
            return XFS_ERROR(EIO);
        }

        src = block_buffer->b_addr;
        copy_start = block_start;
        copylen = block_size;
        if (block_start < offset) {
            copylen = block_size + block_start - offset;
            copy_start = (block_size - copylen) + block_start;
            src = block_buffer->b_addr + (block_size - copylen);
        }
        if ((block_start + block_size) > (offset + len)) {
            copylen = offset + len - copy_start;
        }

        if (copylen > 0) {
            memcpy(buffer, src, copylen);
            buffer += copylen;
            cofs += copylen;
        }
        libxfs_putbuf(block_buffer);
    }
    
    return 0;
}

int extent_overlaps_buffer(xfs_mount_t *mp, xfs_bmbt_irec_t rec, off_t offset, size_t len) {
    size_t extent_size = XFS_FSB_TO_B(mp, rec.br_blockcount);
    size_t extent_start = XFS_FSB_TO_B(mp, rec.br_startoff);
    
    /* check that the extent overlaps the intended read area */
    /* First: the offset lies in the extent */
    if ((extent_start <= offset) && (offset < extent_start + extent_size)) return 1;
    /* Second: the extent start lies in the buffer */
    if ((offset <= extent_start) && (extent_start < offset + len)) return 1;
    return 0;
}

int xfs_readfile_extents(xfs_inode_t *ip, void *buffer, off_t offset, size_t len, int *last_extent) {
    xfs_extnum_t nextents;
    xfs_extnum_t extent;
    xfs_bmbt_irec_t rec;
    xfs_ifork_t *dp = XFS_IFORK_PTR(ip, XFS_DATA_FORK);
    xfs_bmbt_rec_host_t *ep;
	xfs_mount_t	*mp = ip->i_mount;		/* filesystem mount point */
    xfs_fsize_t size = ip->i_d.di_size;

    int error;

    if (offset >= size) return 0;

    if (offset + len > size) 
        len = size - offset;

    nextents = XFS_IFORK_NEXTENTS(ip, XFS_DATA_FORK);
        
    for (extent=0; extent<nextents; extent++) {
        ep = xfs_iext_get_ext(dp, extent);
        xfs_bmbt_get_all(ep, &rec);
                
        if (extent_overlaps_buffer(mp, rec, offset, len)) {
            error = copy_extent_to_buffer(mp, rec, buffer, offset, len); 
            if (error) return error;
        }
    }
    
    return len;
}

int xfs_readfile_btree(xfs_inode_t *ip, void *buffer, off_t offset, size_t len, int *last_extent) {
    xfs_extnum_t nextents;
    xfs_extnum_t extent;
    xfs_ifork_t *dp;
    xfs_bmbt_rec_host_t *ep;
    xfs_bmbt_irec_t rec;
	xfs_mount_t	*mp = ip->i_mount;		/* filesystem mount point */
    xfs_fsize_t size = ip->i_d.di_size;
    
    int error;

    if (offset >= size) return 0;
    
    if (offset + len > size) 
        len = size - offset;
         
    dp = XFS_IFORK_PTR(ip, XFS_DATA_FORK);
    
    if (!(dp->if_flags & XFS_IFEXTENTS) &&
        (error = xfs_iread_extents(NULL, ip, XFS_DATA_FORK)))
        return error;

    nextents = XFS_IFORK_NEXTENTS(ip, XFS_DATA_FORK);

    for (extent=0; extent<nextents; extent++) {
        ep = xfs_iext_get_ext(dp, extent);
        xfs_bmbt_get_all(ep, &rec);
        if (extent_overlaps_buffer(mp, rec, offset, len)) {
            error = copy_extent_to_buffer(mp, rec, buffer, offset, len); 
            if (error) return error;
        }
    }
    
    return len;
}

int xfs_readfile(xfs_inode_t *ip, void *buffer, off_t offset, size_t len, int *last_extent) {
    /* TODO: make this better: */
    /* Initialise the buffer to 0 to handle gaps in extents */
    
    memset(buffer, 0, len);

    if (!(ip->i_d.di_mode & S_IFREG))
		return XFS_ERROR(EINVAL);
    if (XFS_IFORK_FORMAT(ip, XFS_DATA_FORK) == XFS_DINODE_FMT_EXTENTS) {
        return xfs_readfile_extents(ip, buffer, offset, len, last_extent);
    } else if (XFS_IFORK_FORMAT(ip, XFS_DATA_FORK) == XFS_DINODE_FMT_BTREE) {
        return xfs_readfile_btree(ip, buffer, offset, len, last_extent);
    }
    
    return XFS_ERROR(EIO);
}

int xfs_readlink_extents(xfs_inode_t *ip, void *buffer, off_t offset, size_t len, int *last_extent) {
    return xfs_readfile_extents(ip, buffer, offset, len, last_extent);
}

int xfs_readlink_local(xfs_inode_t *ip, void *buffer, off_t offset, size_t len, int *last_extent) {
    xfs_ifork_t *dp;
    xfs_fsize_t size = ip->i_d.di_size;
    dp = XFS_IFORK_PTR(ip, XFS_DATA_FORK);

    if (size - offset <= 0)
        return 0;

    if (size - offset < len)
        len = size - offset;

    memcpy(buffer, dp->if_u1.if_data + offset, len);
    return len;
}

int xfs_readlink(xfs_inode_t *ip, void *buffer, off_t offset, size_t len, int *last_extent) {
    memset(buffer, 0, len);

    if (!(ip->i_d.di_mode & S_IFLNK))
		return XFS_ERROR(EINVAL);
    if (XFS_IFORK_FORMAT(ip, XFS_DATA_FORK) == XFS_DINODE_FMT_EXTENTS) {
        return xfs_readlink_extents(ip, buffer, offset, len, last_extent);
    } else if (XFS_IFORK_FORMAT(ip, XFS_DATA_FORK) == XFS_DINODE_FMT_LOCAL) {
        return xfs_readlink_local(ip, buffer, offset, len, last_extent);
    }
    
    return XFS_ERROR(EIO);
}

struct xfs_name next_name(struct xfs_name current) {
    struct xfs_name name;
    int len = 0;
    
    const char *path = current.name + current.len;
    
    while ((*path) && (*path==PATH_SEP)) path++;
    
    if (!(*path)) {
        name.name = NULL;
        name.len = 0;
        return name;
    }
    
    name.name = path;
    while ((*path) && (*path!=PATH_SEP)) {
        path++;
        len++;
    }
    
    name.len = len;
    return name;
}

struct xfs_name first_name(const char *path) {
    struct xfs_name name;
    
    if (!path) {
        name.name = NULL;
        name.len = 0;
        return name;
    }
    
    name.name = path;
    name.len = 0;
    return next_name(name);
}

int find_path(xfs_mount_t *mp, const char *path, xfs_inode_t **result) {
    xfs_inode_t *current;
    xfs_ino_t inode;
    struct xfs_name xname;
    int error;
   
    error = libxfs_iget(mp, NULL, mp->m_sb.sb_rootino, 0, &current, 0);
    assert(error==0);
    
    xname = first_name(path);
    while (xname.len != 0) {
        if (!(current->i_d.di_mode & S_IFDIR)) {
            libxfs_iput(current, 0);
            return XFS_ERROR(ENOTDIR);
        }
        
        error = libxfs_dir_lookup(NULL, current, &xname, &inode, NULL);
        if (error != 0) {
            return error;
        }

        /* Done with current: make it available */
        libxfs_iput(current, 0);

        error = libxfs_iget(mp, NULL, inode, 0, &current, 0);
        if (error != 0) {
            printf("Failed to get inode for %s %d\n", xname.name, xname.len);
            return XFS_ERROR(EIO);
        }
        xname = next_name(xname);
    }
    *result = current;
    return 0;
}

int xfs_stat(xfs_inode_t *inode, struct stat *stats) {  
    stats->st_dev = 0;
    stats->st_mode = inode->i_d.di_mode;
    stats->st_nlink = inode->i_d.di_nlink;
    stats->st_ino = inode->i_ino;
    stats->st_uid = inode->i_d.di_uid;
    stats->st_gid = inode->i_d.di_gid;
    stats->st_rdev = 0;
    stats->st_atimespec.tv_sec = inode->i_d.di_atime.t_sec;
    stats->st_atimespec.tv_nsec = inode->i_d.di_atime.t_nsec;
    stats->st_mtimespec.tv_sec = inode->i_d.di_mtime.t_sec;
    stats->st_mtimespec.tv_nsec = inode->i_d.di_mtime.t_nsec;
    stats->st_ctimespec.tv_sec = inode->i_d.di_ctime.t_sec;
    stats->st_ctimespec.tv_nsec = inode->i_d.di_ctime.t_nsec;
    stats->st_birthtimespec.tv_sec = inode->i_d.di_ctime.t_sec; 
    stats->st_birthtimespec.tv_nsec = inode->i_d.di_ctime.t_nsec; 
    stats->st_size = inode->i_d.di_size;
    stats->st_blocks = inode->i_d.di_nblocks;
    stats->st_blksize = 4096;
    stats->st_flags = inode->i_d.di_flags;
    stats->st_gen = inode->i_d.di_gen;
    return 0;
}

int xfs_is_dir(xfs_inode_t *inode) {
    return S_ISDIR(inode->i_d.di_mode);
}

int xfs_is_link(xfs_inode_t *inode) {
    return S_ISLNK(inode->i_d.di_mode);
}

int xfs_is_regular(xfs_inode_t *inode) {
    return S_ISREG(inode->i_d.di_mode);
}

xfs_mount_t *mount_xfs(char *progname, char *source_name) {
    xfs_mount_t	*mp;
    xfs_buf_t	*sbp;
    xfs_sb_t	*sb;
    libxfs_init_t	xargs;
    xfs_mount_t	*mbuf = (xfs_mount_t *)malloc(sizeof(xfs_mount_t));
    
    /* prepare the libxfs_init structure */
    
    memset(&xargs, 0, sizeof(xargs));
    xargs.isdirect = LIBXFS_DIRECT;
    xargs.isreadonly = LIBXFS_ISREADONLY;
    
    if (1)  {
        xargs.dname = source_name;
        xargs.disfile = 1;
    } else
        xargs.volname = source_name;
    
    if (!libxfs_init(&xargs))  {
        do_log(_("%s: couldn't initialize XFS library\n"
                 "%s: Aborting.\n"), progname, progname);
        return NULL;
    }
    
    /* prepare the mount structure */
    
    sbp = libxfs_readbuf(xargs.ddev, XFS_SB_DADDR, 1, 0);
    memset(mbuf, 0, sizeof(xfs_mount_t));
    sb = &(mbuf->m_sb);
    libxfs_sb_from_disk(sb, XFS_BUF_TO_SBP(sbp));
    
    mp = libxfs_mount(mbuf, sb, xargs.ddev, xargs.logdev, xargs.rtdev, 1);
    if (mp == NULL) {
        do_log(_("%s: %s filesystem failed to initialize\n"
                 "%s: Aborting.\n"), progname, source_name, progname);
        return NULL;
    } else if (mp->m_sb.sb_inprogress)  {
        do_log(_("%s %s filesystem failed to initialize\n"
                 "%s: Aborting.\n"), progname, source_name, progname);
        return NULL;
    } else if (mp->m_sb.sb_logstart == 0)  {
        do_log(_("%s: %s has an external log.\n%s: Aborting.\n"),
               progname, source_name, progname);
        return NULL;
	} else if (mp->m_sb.sb_rextents != 0)  {
        do_log(_("%s: %s has a real-time section.\n"
                 "%s: Aborting.\n"), progname, source_name, progname);
        return NULL;
    }
    
    return mp;
}
