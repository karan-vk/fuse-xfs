/*
 * Copyright (c) 2000-2003,2005 Silicon Graphics, Inc.
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
 * Reservation functions here avoid a huge stack in xfs_trans_init
 * due to register overflow from temporaries in the calculations.
 */

STATIC uint
xfs_calc_write_reservation(xfs_mount_t *mp)
{
	return XFS_CALC_WRITE_LOG_RES(mp) + XFS_DQUOT_LOGRES(mp);
}

STATIC uint
xfs_calc_itruncate_reservation(xfs_mount_t *mp)
{
	return XFS_CALC_ITRUNCATE_LOG_RES(mp) + XFS_DQUOT_LOGRES(mp);
}

STATIC uint
xfs_calc_rename_reservation(xfs_mount_t *mp)
{
	return XFS_CALC_RENAME_LOG_RES(mp) + XFS_DQUOT_LOGRES(mp);
}

STATIC uint
xfs_calc_link_reservation(xfs_mount_t *mp)
{
	return XFS_CALC_LINK_LOG_RES(mp) + XFS_DQUOT_LOGRES(mp);
}

STATIC uint
xfs_calc_remove_reservation(xfs_mount_t *mp)
{
	return XFS_CALC_REMOVE_LOG_RES(mp) + XFS_DQUOT_LOGRES(mp);
}

STATIC uint
xfs_calc_symlink_reservation(xfs_mount_t *mp)
{
	return XFS_CALC_SYMLINK_LOG_RES(mp) + XFS_DQUOT_LOGRES(mp);
}

STATIC uint
xfs_calc_create_reservation(xfs_mount_t *mp)
{
	return XFS_CALC_CREATE_LOG_RES(mp) + XFS_DQUOT_LOGRES(mp);
}

STATIC uint
xfs_calc_mkdir_reservation(xfs_mount_t *mp)
{
	return XFS_CALC_MKDIR_LOG_RES(mp) + XFS_DQUOT_LOGRES(mp);
}

STATIC uint
xfs_calc_ifree_reservation(xfs_mount_t *mp)
{
	return XFS_CALC_IFREE_LOG_RES(mp) + XFS_DQUOT_LOGRES(mp);
}

STATIC uint
xfs_calc_ichange_reservation(xfs_mount_t *mp)
{
	return XFS_CALC_ICHANGE_LOG_RES(mp) + XFS_DQUOT_LOGRES(mp);
}

STATIC uint
xfs_calc_growdata_reservation(xfs_mount_t *mp)
{
	return XFS_CALC_GROWDATA_LOG_RES(mp);
}

STATIC uint
xfs_calc_growrtalloc_reservation(xfs_mount_t *mp)
{
	return XFS_CALC_GROWRTALLOC_LOG_RES(mp);
}

STATIC uint
xfs_calc_growrtzero_reservation(xfs_mount_t *mp)
{
	return XFS_CALC_GROWRTZERO_LOG_RES(mp);
}

STATIC uint
xfs_calc_growrtfree_reservation(xfs_mount_t *mp)
{
	return XFS_CALC_GROWRTFREE_LOG_RES(mp);
}

STATIC uint
xfs_calc_swrite_reservation(xfs_mount_t *mp)
{
	return XFS_CALC_SWRITE_LOG_RES(mp);
}

STATIC uint
xfs_calc_writeid_reservation(xfs_mount_t *mp)
{
	return XFS_CALC_WRITEID_LOG_RES(mp);
}

STATIC uint
xfs_calc_addafork_reservation(xfs_mount_t *mp)
{
	return XFS_CALC_ADDAFORK_LOG_RES(mp) + XFS_DQUOT_LOGRES(mp);
}

STATIC uint
xfs_calc_attrinval_reservation(xfs_mount_t *mp)
{
	return XFS_CALC_ATTRINVAL_LOG_RES(mp);
}

STATIC uint
xfs_calc_attrset_reservation(xfs_mount_t *mp)
{
	return XFS_CALC_ATTRSET_LOG_RES(mp) + XFS_DQUOT_LOGRES(mp);
}

STATIC uint
xfs_calc_attrrm_reservation(xfs_mount_t *mp)
{
	return XFS_CALC_ATTRRM_LOG_RES(mp) + XFS_DQUOT_LOGRES(mp);
}

STATIC uint
xfs_calc_clear_agi_bucket_reservation(xfs_mount_t *mp)
{
	return XFS_CALC_CLEAR_AGI_BUCKET_LOG_RES(mp);
}

/*
 * Initialize the precomputed transaction reservation values
 * in the mount structure.
 */
void
xfs_trans_init(
	xfs_mount_t	*mp)
{
	xfs_trans_reservations_t	*resp;

	resp = &(mp->m_reservations);
	resp->tr_write = xfs_calc_write_reservation(mp);
	resp->tr_itruncate = xfs_calc_itruncate_reservation(mp);
	resp->tr_rename = xfs_calc_rename_reservation(mp);
	resp->tr_link = xfs_calc_link_reservation(mp);
	resp->tr_remove = xfs_calc_remove_reservation(mp);
	resp->tr_symlink = xfs_calc_symlink_reservation(mp);
	resp->tr_create = xfs_calc_create_reservation(mp);
	resp->tr_mkdir = xfs_calc_mkdir_reservation(mp);
	resp->tr_ifree = xfs_calc_ifree_reservation(mp);
	resp->tr_ichange = xfs_calc_ichange_reservation(mp);
	resp->tr_growdata = xfs_calc_growdata_reservation(mp);
	resp->tr_swrite = xfs_calc_swrite_reservation(mp);
	resp->tr_writeid = xfs_calc_writeid_reservation(mp);
	resp->tr_addafork = xfs_calc_addafork_reservation(mp);
	resp->tr_attrinval = xfs_calc_attrinval_reservation(mp);
	resp->tr_attrset = xfs_calc_attrset_reservation(mp);
	resp->tr_attrrm = xfs_calc_attrrm_reservation(mp);
	resp->tr_clearagi = xfs_calc_clear_agi_bucket_reservation(mp);
	resp->tr_growrtalloc = xfs_calc_growrtalloc_reservation(mp);
	resp->tr_growrtzero = xfs_calc_growrtzero_reservation(mp);
	resp->tr_growrtfree = xfs_calc_growrtfree_reservation(mp);
}

/*
 * Roll from one trans in the sequence of PERMANENT transactions to
 * the next: permanent transactions are only flushed out when
 * committed with XFS_TRANS_RELEASE_LOG_RES, but we still want as soon
 * as possible to let chunks of it go to the log. So we commit the
 * chunk we've been working on and get a new transaction to continue.
 */
int
xfs_trans_roll(
	struct xfs_trans	**tpp,
	struct xfs_inode	*dp)
{
	struct xfs_trans	*trans;
	unsigned int		logres, count;
	int			error;

	/*
	 * Ensure that the inode is always logged.
	 */
	trans = *tpp;
	xfs_trans_log_inode(trans, dp, XFS_ILOG_CORE);

	/*
	 * Copy the critical parameters from one trans to the next.
	 */
	logres = trans->t_log_res;
	count = trans->t_log_count;
	*tpp = xfs_trans_dup(trans);

	/*
	 * Commit the current transaction.
	 * If this commit failed, then it'd just unlock those items that
	 * are not marked ihold. That also means that a filesystem shutdown
	 * is in progress. The caller takes the responsibility to cancel
	 * the duplicate transaction that gets returned.
	 */
	error = xfs_trans_commit(trans, 0);
	if (error)
		return (error);

	trans = *tpp;

	/*
	 * Reserve space in the log for th next transaction.
	 * This also pushes items in the "AIL", the list of logged items,
	 * out to disk if they are taking up space at the tail of the log
	 * that we want to use.  This requires that either nothing be locked
	 * across this call, or that anything that is locked be logged in
	 * the prior and the next transactions.
	 */
	error = xfs_trans_reserve(trans, 0, logres, 0,
				  XFS_TRANS_PERM_LOG_RES, count);
	/*
	 *  Ensure that the inode is in the new transaction and locked.
	 */
	if (error)
		return error;

	xfs_trans_ijoin(trans, dp, XFS_ILOCK_EXCL);
	xfs_trans_ihold(trans, dp);
	return 0;
}

