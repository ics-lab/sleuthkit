#include "tsk_xfs.h"

/*
 * Check the validity of the SB found.
 * XFS docs; Chapter 2. Metadata Integrity
 */
STATIC int
xfs_mount_validate_sb(
    xfs_sb_t    *sbp)
{
    if (tsk_fs_guessu64(sbp->sb_magicnum, XFS_SB_MAGIC)) {
        tsk_error_reset();
        tsk_error_set_errno(TSK_ERR_FS_MAGIC);
        tsk_error_set_errstr("xfs: Invalid magic number");
        return 0;
    }

    if (!xfs_sb_good_version(sbp)) {
        tsk_error_reset();
        tsk_error_set_errno(TSK_ERR_FS_UNSUPTYPE);
        tsk_error_set_errstr("xfs: unsupported superblock version or corrupted superblock");
        return 0;
    }

    /*
     * Version 5 superblock feature mask validation. Reject combinations the
     * kernel cannot support up front before checking anything else. For
     * write validation, we don't need to check feature masks.
     */
    if (tsk_fs_guessu64(sbp->sb_versionnum, XFS_SB_VERSION_5)) {
        if (xfs_sb_has_compat_feature(sbp, XFS_SB_FEAT_COMPAT_UNKNOWN)) {
            tsk_fprintf(stderr,
                "Superblock has unknown compatible features (0x%x) enabled.",
                (sbp->sb_features_compat & XFS_SB_FEAT_COMPAT_UNKNOWN));
            tsk_fprintf(stderr, "Using a more recent kernel is recommended.");
        }

        if (xfs_sb_has_ro_compat_feature(sbp, XFS_SB_FEAT_RO_COMPAT_UNKNOWN)) {
            tsk_fprintf(stderr, 
                "Superblock has unknown read-only compatible features (0x%x) enabled.",
                (sbp->sb_features_ro_compat & XFS_SB_FEAT_RO_COMPAT_UNKNOWN));
        }

        if (xfs_sb_has_incompat_feature(sbp, XFS_SB_FEAT_INCOMPAT_UNKNOWN)) {
            tsk_fprintf(stderr, 
                "Superblock has unknown incompatible features (0x%x) enabled.",
                (sbp->sb_features_incompat & XFS_SB_FEAT_INCOMPAT_UNKNOWN));
            tsk_fprintf(stderr, 
                "Filesystem can not be safely mounted by tsk.");
        }
    } else if (xfs_sb_version_hascrc(sbp)) {
        /*
         * We can't read verify the sb LSN because the read verifier is
         * called before the log is allocated and processed. We know the
         * log is set up before write verifier (!check_version) calls,
         * so just check it here.
         */
        /*if (!xfs_log_check_lsn(mp, sbp->sb_lsn)) // sb_lsn == 64bit uint64_t
            return -EFSCORRUPTED;*/
    }

    /*
    if (xfs_sb_version_has_pquotino(sbp)) {
        if (sbp->sb_qflags & (XFS_OQUOTA_ENFD | XFS_OQUOTA_CHKD)) {
            xfs_notice(mp,
               "Version 5 of Super block has XFS_OQUOTA bits.");
            return -EFSCORRUPTED;
        }
    } else if (sbp->sb_qflags & (XFS_PQUOTA_ENFD | XFS_GQUOTA_ENFD |
                XFS_PQUOTA_CHKD | XFS_GQUOTA_CHKD)) {
            xfs_notice(mp,
"Superblock earlier than Version 5 has XFS_[PQ]UOTA_{ENFD|CHKD} bits.");
            return -EFSCORRUPTED;
    }*/

    /*
     * Full inode chunks must be aligned to inode chunk size when
     * sparse inodes are enabled to support the sparse chunk
     * allocation algorithm and prevent overlapping inode records.
     */
    /*
    if (xfs_sb_version_hassparseinodes(sbp)) {
        uint32_t    align;

        align = XFS_INODES_PER_CHUNK * sbp->sb_inodesize
                >> sbp->sb_blocklog;
        if (sbp->sb_inoalignmt != align) {
            xfs_warn(mp,
"Inode block alignment (%u) must match chunk size (%u) for sparse inodes.",
                 sbp->sb_inoalignmt, align);
            return -EINVAL;
        }
    }*/

    /*
    if (unlikely(
        sbp->sb_logstart == 0 && mp->m_logdev_targp == mp->m_ddev_targp)) {
        xfs_warn(mp,
        "filesystem is marked as having an external log; "
        "specify logdev on the mount command line.");
        return -EINVAL;
    }

    if (unlikely(
        sbp->sb_logstart != 0 && mp->m_logdev_targp != mp->m_ddev_targp)) {
        xfs_warn(mp,
        "filesystem is marked as having an internal log; "
        "do not specify logdev on the mount command line.");
        return -EINVAL;
    }
    */

    /*
     * More sanity checking.  Most of these were stolen directly from
     * xfs_repair.
     */
    if ((tsk_getu32(sbp->sb_agcount) <= 0                    ||
        tsk_getu16(sbp->sb_sectsize) < XFS_MIN_SECTORSIZE           ||
        tsk_getu16(sbp->sb_sectsize) > XFS_MAX_SECTORSIZE           ||
        tsk_getu8(sbp->sb_sectlog) < XFS_MIN_SECTORSIZE_LOG            ||
        tsk_getu8(sbp->sb_sectlog) > XFS_MAX_SECTORSIZE_LOG            ||
        tsk_getu16(sbp->sb_sectsize) != (1 << tsk_getu8(sbp->sb_sectlog))          ||
        tsk_getu32(sbp->sb_blocksize) < XFS_MIN_BLOCKSIZE           ||
        tsk_getu32(sbp->sb_blocksize) > XFS_MAX_BLOCKSIZE           ||
        tsk_getu8(sbp->sb_blocklog) < XFS_MIN_BLOCKSIZE_LOG            ||
        tsk_getu8(sbp->sb_blocklog) > XFS_MAX_BLOCKSIZE_LOG            ||
        tsk_getu32(sbp->sb_blocksize) != (1 << tsk_getu8(sbp->sb_blocklog))        ||
        tsk_getu8(sbp->sb_dirblklog) + tsk_getu8(sbp->sb_blocklog) > XFS_MAX_BLOCKSIZE_LOG ||
        tsk_getu16(sbp->sb_inodesize) < XFS_DINODE_MIN_SIZE         ||
        tsk_getu16(sbp->sb_inodesize) > XFS_DINODE_MAX_SIZE         ||
        tsk_getu8(sbp->sb_inodelog) < XFS_DINODE_MIN_LOG           ||
        tsk_getu8(sbp->sb_inodelog) > XFS_DINODE_MAX_LOG           ||
        tsk_getu16(sbp->sb_inodesize) != (1 << tsk_getu8(sbp->sb_inodelog))        ||
        tsk_getu32(sbp->sb_logsunit) > XLOG_MAX_RECORD_BSIZE            ||
        tsk_getu16(sbp->sb_inopblock) != howmany(tsk_getu16(sbp->sb_blocksize), tsk_getu16(sbp->sb_inodesize)) ||
        (tsk_getu8(sbp->sb_blocklog) - tsk_getu8(sbp->sb_inodelog) != tsk_getu8(sbp->sb_inopblog))   ||
        (tsk_getu32(sbp->sb_rextsize) * tsk_getu32(sbp->sb_blocksize) > XFS_MAX_RTEXTSIZE)  ||
        (tsk_getu32(sbp->sb_rextsize) * tsk_getu32(sbp->sb_blocksize) < XFS_MIN_RTEXTSIZE)  ||
        (tsk_getu8(sbp->sb_imax_pct) > 100 /* zero sb_imax_pct is valid */)    ||
        tsk_getu64(sbp->sb_dblocks) == 0                    ||
        tsk_getu64(sbp->sb_dblocks) > XFS_MAX_DBLOCKS(sbp)          ||
        tsk_getu64(sbp->sb_dblocks) < XFS_MIN_DBLOCKS(sbp)          ||
        tsk_getu8(sbp->sb_shared_vn) != 0)) {
        tsk_fprintf(stderr, "Superblock sanity check failed");
        return -EFSCORRUPTED;
    }

    if (tsk_fs_guessu64(sbp->sb_versionnum, XFS_SB_VERSION_5) &&
        tsk_getu32(sbp->sb_blocksize) < XFS_MIN_CRC_BLOCKSIZE) {
        tsk_fprintf(stderr, "v5 Superblock sanity check failed");
        return -EFSCORRUPTED;
    }

    /*
     * disabled:: verify machine-dependency code
     * Until this is fixed only page-sized or smaller data blocks work.
     */
    /*
    if (unlikely(sbp->sb_blocksize > PAGE_SIZE)) {
        xfs_warn(mp,
        "File system with blocksize %d bytes. "
        "Only pagesize (%ld) or less will currently work.",
                sbp->sb_blocksize, PAGE_SIZE);
        return -ENOSYS;
    }
    */

    /*
     * Currently only very few inode sizes are supported.
     */
    switch (sbp->sb_inodesize) {
    case 256:
    case 512:
    case 1024:
    case 2048:
        break;
    default:
        tsk_fprintf(mp, "inode size of %d bytes not supported",
                tsk_getu16(sbp->sb_inodesize));
        return -ENOSYS;
    }

    /*
    if (xfs_sb_validate_fsb_count(sbp, sbp->sb_dblocks) ||
        xfs_sb_validate_fsb_count(sbp, sbp->sb_rblocks)) {
        xfs_warn(mp,
        "file system too large to be mounted on this system.");
        return -EFBIG;
    }*/

    return 0;
}

/**
 * \internal
 * Open part of a disk image as a XFS file system.
 *
 * @param img_info Disk image to analyze
 * @param offset Byte offset where file system starts
 * @param ftype Specific type of file system
 * @param test NOT USED
 * @returns NULL on error or if data is not an XFS file system
 */
TSK_FS_INFO *
xfs_open(TSK_IMG_INFO * img_info, TSK_OFF_T offset,
    TSK_FS_TYPE_ENUM ftype, uint8_t test)
{
    unsigned int len;
    TSK_FS_INFO *fs;
    ssize_t cnt;

    // clean up any error messages that are lying around
    tsk_error_reset();

    if (TSK_FS_TYPE_ISXFS(ftype) == 0) {
        tsk_error_reset();
        tsk_error_set_errno(TSK_ERR_FS_ARG);
        tsk_error_set_errstr("Invalid FS Type in xfs_open");
        return NULL;
    }

    if (img_info->sector_size == 0) {
        tsk_error_reset();
        tsk_error_set_errno(TSK_ERR_FS_ARG);
        tsk_error_set_errstr("xfs_open: sector size is 0");
        return NULL;
    }

    if ((xfs = (XFS_INFO *) tsk_fs_malloc(sizeof(*xfs))) == NULL)
        return NULL;

    fs = &(xfs->fs_info);

    fs->ftype = ftype;
    fs->flags = 0;
    fs->img_info = img_info;
    fs->offset = offset;
    fs->tag = TSK_FS_INFO_TAG;

    /*
     * Read the superblock.
     */
    len = sizeof(xfs_dsb);
    if ((xfs->fs = (xfs_dsb *) tsk_malloc(len)) == NULL) {
        fs->tag = 0;
        tsk_fs_free((TSK_FS_INFO *)xfs);
        return NULL;
    }

    cnt = tsk_fs_read(fs, XFS_SBOFF, (char *) xfs->fs, len);
    if (cnt != len) {
        if (cnt >= 0) {
            tsk_error_reset();
            tsk_error_set_errno(TSK_ERR_FS_READ);
        }
        tsk_error_set_errstr2("xfs_open: superblock");
        fs->tag = 0;
        free(xfs->fs);
        tsk_fs_free((TSK_FS_INFO *)xfs);
        return NULL;
    }

    if (xfs_mount_validate_sb(xfs->fs))
    {
        free(xfs->fs);
        tsk_fs_free((TSK_FS_INFO *)xfs);
        tsk_error_reset();
        tsk_error_set_errno(TSK_ERR_FS_CORRUPT);
        tsk_error_set_errstr("xfs_open: superblock validation failed");
        if (tsk_verbose)
            fprintf(stderr, "xfs_open: invalid superblock\n");
        return NULL;
    }

    /*
     * Calculate the meta data info
     */
    fs->inum_count = tsk_getu64(fs->endian, xfs->fs->sb_icount);
    fs->last_inum = tsk_getu64(fs->inum_count);
    fs->first_inum = XFS_FIRSTINO;
    fs->root_inum = tsk_getu64(fs->sb_rootino);

    if (tsk_getu64(fs->sb_icount) < 10) {
        fs->tag = 0;
        free(xfs->fs);
        tsk_fs_free((TSK_FS_INFO *)xfs);
        tsk_error_reset();
        tsk_error_set_errno(TSK_ERR_FS_MAGIC);
        tsk_error_set_errstr("Not an XFS file system (inum count)");
        if (tsk_verbose)
            fprintf(stderr, "xfs_open: two few inodes\n");
        return NULL;
    }

    /* Set the size of the inode, but default to our data structure
     * size if it is larger */
    xfs->inode_size = tsk_getu16(fs->endian, xfs->fs->sb_inodesize);
    if (xfs->inode_size < sizeof(xfs_dinode_t)) {
        if (tsk_verbose)
            tsk_fprintf(stderr, "SB inode size is small");
    }

    /*
     * Calculate the block info
     */
    fs->dev_bsize = img_info->sector_size;
    /* journaling
    if (tsk_getu32(fs->endian,
            xfs->fs->
            s_feature_incompat) & EXT2FS_FEATURE_INCOMPAT_64BIT) {
//        printf("DEBUG fs_open: 64bit file system\n");
        fs->block_count =
            ext4_getu64(fs->endian, ext2fs->fs->s_blocks_count_hi,
            ext2fs->fs->s_blocks_count);
    }
    else {
        fs->block_count =
            tsk_getu32(fs->endian, ext2fs->fs->s_blocks_count);
    }*/
    fs->first_block = 0;
    fs->last_block_act = fs->last_block = tsk_getu64(fs->sb_dblocks) - 1;
    /*
    xfs->first_data_block =
        tsk_getu32(fs->endian, ext2fs->fs->s_first_data_block);

    if (tsk_getu32(fs->endian, ext2fs->fs->s_log_block_size) !=
        tsk_getu32(fs->endian, ext2fs->fs->s_log_frag_size)) {
        fs->tag = 0;
        free(ext2fs->fs);
        tsk_fs_free((TSK_FS_INFO *)ext2fs);
        tsk_error_reset();
        tsk_error_set_errno(TSK_ERR_FS_UNSUPFUNC);
        tsk_error_set_errstr
            ("This file system has fragments that are a different size than blocks, which is not currently supported\nContact brian with details of the system that created this image");
        if (tsk_verbose)
            fprintf(stderr,
                "xfs_open: fragment size not equal to block size\n");
        return NULL;
    }
    
    fs->block_size =
        XFS_MIN_BLOCKSIZE << tsk_getu32(fs->endian,
        xfs->fs->sb_blocklog);
    */
    // determine the last block we have in this image
    /*
    if ((TSK_DADDR_T) ((img_info->size - offset) / fs->block_size) <
        fs->block_count)
        fs->last_block_act =
            (img_info->size - offset) / fs->block_size - 1;
    */

    /* The group descriptors are located in the block following the
     * super block */
    /*
    xfs->groups_offset =
        roundup((XFS_SBOFF + sizeof(xfs_sb)), fs->block_size);

    // sanity check to avoid divide by zero issues
    if (tsk_getu32(fs->endian, ext2fs->fs->s_blocks_per_group) == 0) {
        fs->tag = 0;
        free(ext2fs->fs);
        tsk_fs_free((TSK_FS_INFO *)ext2fs);
        tsk_error_reset();
        tsk_error_set_errno(TSK_ERR_FS_MAGIC);
        tsk_error_set_errstr("Not an EXTxFS file system (blocks per group)");
        if (tsk_verbose)
            fprintf(stderr, "ext2fs_open: blocks per group is 0\n");
        return NULL;
    }
    if (tsk_getu32(fs->endian, ext2fs->fs->s_inodes_per_group) == 0) {
        fs->tag = 0;
        free(ext2fs->fs);
        tsk_fs_free((TSK_FS_INFO *)ext2fs);
        tsk_error_reset();
        tsk_error_set_errno(TSK_ERR_FS_MAGIC);
        tsk_error_set_errstr("Not an EXTxFS file system (inodes per group)");
        if (tsk_verbose)
            fprintf(stderr, "ext2fs_open: inodes per group is 0\n");
        return NULL;
    }

    if (tsk_getu32(fs->endian,
            ext2fs->fs->
            s_feature_incompat) & EXT2FS_FEATURE_INCOMPAT_64BIT) {
        ext2fs->groups_count =
            (EXT2_GRPNUM_T) ((ext4_getu64(fs->endian,
                    ext2fs->fs->s_blocks_count_hi,
                    ext2fs->fs->s_blocks_count)
                - ext2fs->first_data_block + tsk_getu32(fs->endian,
                    ext2fs->fs->s_blocks_per_group) - 1)
            / tsk_getu32(fs->endian, ext2fs->fs->s_blocks_per_group));
    }
    else {
        xfs->groups_count =
            (EXT2_GRPNUM_T) ((tsk_getu32(fs->endian,
                    ext2fs->fs->s_blocks_count) -
                ext2fs->first_data_block + tsk_getu32(fs->endian,
                    ext2fs->fs->s_blocks_per_group) -
                1) / tsk_getu32(fs->endian,
                ext2fs->fs->s_blocks_per_group));
    }
    */
    /* Volume ID */
    //for (fs->fs_id_used = 0; fs->fs_id_used < 16; fs->fs_id_used++) {
    //    fs->fs_id[fs->fs_id_used] = ext2fs->fs->s_uuid[fs->fs_id_used];
    //}

    /* Set the generic function pointers */
    fs->inode_walk = xfs_inode_walk; // uint8_t xfs_inode_walk() 구현
    fs->block_walk = xfs_block_walk; // 
    fs->block_getflags = xfs_block_getflags;

    fs->get_default_attr_type = tsk_fs_unix_get_default_attr_type;
    fs->load_attrs = xfs_load_attrs;

    fs->file_add_meta = xfs_inode_lookup;
    fs->dir_open_meta = xfs_dir_open_meta;
    fs->fsstat = xfs_fsstat;
    fs->fscheck = xfs_fscheck;
    fs->istat = xfs_istat;
    fs->name_cmp = tsk_fs_unix_name_cmp;
    fs->close = xfs_close;

    /* Journal */
    //fs->journ_inum = tsk_getu32(fs->endian, xfs->fs->s_journal_inum);
    //fs->jblk_walk = xfs_jblk_walk;
    //fs->jentry_walk = xfs_jentry_walk;
    //fs->jopen = xfs_jopen;

    /* initialize the caches */
    /* inode map */
    //xfs->imap_buf = NULL;
    //xfs->imap_grp_num = 0xffffffff;

    /* block map */
    //xfs->bmap_buf = NULL;
    //xfs->bmap_grp_num = 0xffffffff;

    /* group descriptor */
    //xfs->grp_buf = NULL;
    //xfs->grp_num = 0xffffffff;
    

    /*
     * Print some stats.
     */
    if (tsk_verbose)
        tsk_fprintf(stderr,
            "inodes %" PRIu32 " root ino %" PRIuINUM " blocks %" PRIu32
            " inodes/block %" PRIu32 "\n", tsk_getu64(fs->endian,
                xfs->fs->sb_icount),
            fs->root_inum, tsk_getu64(fs->endian,
                xfs->fs->sb_dblocks), tsk_getu16(fs->endian,
                xfs->fs->sb_inopblock));

    tsk_init_lock(&xfs->lock);

    return (fs);
    // end: 확인 후 수정

}


TSK_FS_BLOCK_FLAG_ENUM
xfs_block_getflags(TSK_FS_INFO * a_fs, TSK_DADDR_T a_addr)
{
    EXT2FS_INFO *ext2fs = (EXT2FS_INFO *) a_fs;
    int flags;
    EXT2_GRPNUM_T grp_num;
    TSK_DADDR_T dbase = 0;      /* first block number in group */
    TSK_DADDR_T dmin = 0;       /* first block after inodes */

    // these blocks are not described in the group descriptors
    // sparse
    if (a_addr == 0)
        return TSK_FS_BLOCK_FLAG_CONT | TSK_FS_BLOCK_FLAG_ALLOC;
    if (a_addr < ext2fs->first_data_block)
        return TSK_FS_BLOCK_FLAG_META | TSK_FS_BLOCK_FLAG_ALLOC;

    grp_num = ext2_dtog_lcl(a_fs, ext2fs->fs, a_addr);

    /* lock access to bmap_buf */
    tsk_take_lock(&ext2fs->lock);

    /* Lookup bitmap if not loaded */
    if (ext2fs_bmap_load(ext2fs, grp_num)) {
        tsk_release_lock(&ext2fs->lock);
        return 0;
    }

    /*
     * Be sure to use the right group descriptor information. XXX There
     * appears to be an off-by-one discrepancy between bitmap offsets and
     * disk block numbers.
     *
     * Addendum: this offset is controlled by the super block's
     * s_first_data_block field.
     */
    dbase = ext2_cgbase_lcl(a_fs, ext2fs->fs, grp_num);
    flags = (isset(ext2fs->bmap_buf, a_addr - dbase) ?
        TSK_FS_BLOCK_FLAG_ALLOC : TSK_FS_BLOCK_FLAG_UNALLOC);
    
    /*
     *  Identify meta blocks
     * (any blocks that can't be allocated for file/directory data).
     *
     * XXX With sparse superblock placement, most block groups have the
     * block and inode bitmaps where one would otherwise find the backup
     * superblock and the backup group descriptor blocks. The inode
     * blocks are in the normal place, though. This leaves little gaps
     * between the bitmaps and the inode table - and ext2fs will use
     * those blocks for file/directory data blocks. So we must properly
     * account for those gaps between meta blocks.
     *
     * Thus, superblocks and group descriptor blocks are sometimes overlaid
     * by bitmap blocks. This means that one can still assume that the
     * locations of superblocks and group descriptor blocks are reserved.
     * They just happen to be reserved for something else :-)
     */

    if (ext2fs->ext4_grp_buf != NULL) {
        dmin = ext4_getu64(a_fs->endian, ext2fs->ext4_grp_buf->bg_inode_table_hi,
                    ext2fs->ext4_grp_buf->bg_inode_table_lo) + + INODE_TABLE_SIZE(ext2fs);

        if ((a_addr >= dbase
                && a_addr < ext4_getu64(a_fs->endian, 
                ext2fs->ext4_grp_buf->bg_block_bitmap_hi,
                ext2fs->ext4_grp_buf->bg_block_bitmap_lo))
            || (a_addr == ext4_getu64(a_fs->endian, 
                ext2fs->ext4_grp_buf->bg_block_bitmap_hi,
                ext2fs->ext4_grp_buf->bg_block_bitmap_lo))
            || (a_addr == ext4_getu64(a_fs->endian, 
                ext2fs->ext4_grp_buf->bg_inode_bitmap_hi,
                ext2fs->ext4_grp_buf->bg_inode_bitmap_lo))
            || (a_addr >= ext4_getu64(a_fs->endian, 
                ext2fs->ext4_grp_buf->bg_inode_table_hi,
                ext2fs->ext4_grp_buf->bg_inode_table_lo)
                && a_addr < dmin))
            flags |= TSK_FS_BLOCK_FLAG_META;
        else
            flags |= TSK_FS_BLOCK_FLAG_CONT;

    }
    else {
        dmin =
            tsk_getu32(a_fs->endian,
            ext2fs->grp_buf->bg_inode_table) + INODE_TABLE_SIZE(ext2fs);

        if ((a_addr >= dbase
                && a_addr < tsk_getu32(a_fs->endian,
                    ext2fs->grp_buf->bg_block_bitmap))
            || (a_addr == tsk_getu32(a_fs->endian,
                    ext2fs->grp_buf->bg_block_bitmap))
            || (a_addr == tsk_getu32(a_fs->endian,
                    ext2fs->grp_buf->bg_inode_bitmap))
            || (a_addr >= tsk_getu32(a_fs->endian,
                    ext2fs->grp_buf->bg_inode_table)
                && a_addr < dmin))
            flags |= TSK_FS_BLOCK_FLAG_META;
        else
            flags |= TSK_FS_BLOCK_FLAG_CONT;
    }
    
    tsk_release_lock(&ext2fs->lock);
    return (TSK_FS_BLOCK_FLAG_ENUM)flags;
}

/** \internal
 * Test if block group has a super block in it.
 *
 * @return 1 if block group has superblock, otherwise 0
*/
static uint32_t
xfs_is_super_bg(uint32_t feature_ro_compat, uint32_t group_block)
{
    // if no sparse feature, then it has super block
    if (!(feature_ro_compat & EXT2FS_FEATURE_RO_COMPAT_SPARSE_SUPER))
        return 1;

    // group 0 always has super block
    if (group_block == 0) 
        return 1;

    // Sparse FS put super blocks in groups that are powers of 3, 5, 7
    if (test_root(group_block, 3) || 
            (test_root(group_block, 5)) ||
            (test_root(group_block, 7))) {
        return 1;
    }

    return 0;
}


/** \internal
 * Add a single extent -- that is, a single data ran -- to the file data attribute.
 * @return 0 on success, 1 on error.
 */
static TSK_OFF_T
xfs_make_data_run_extent(TSK_FS_INFO * fs_info, TSK_FS_ATTR * fs_attr,
    xfs_extent * extent)
{
    TSK_FS_ATTR_RUN *data_run;
    data_run = tsk_fs_attr_run_alloc();
    if (data_run == NULL) {
        return 1;
    }

    data_run->offset = tsk_getu32(fs_info->endian, extent->ee_block);
    data_run->addr =
        (((uint32_t) tsk_getu16(fs_info->endian,
                extent->ee_start_hi)) << 16) | tsk_getu32(fs_info->endian,
        extent->ee_start_lo);
    data_run->len = tsk_getu16(fs_info->endian, extent->ee_len);

    // save the run
    if (tsk_fs_attr_add_run(fs_info, fs_attr, data_run)) {
        return 1;
    }

    return 0;
}


/** \internal
 * Given a block that contains an extent node (which starts with extent_header),
 * walk it, and add everything encountered to the appropriate attributes.
 * @return 0 on success, 1 on error.
 */
static TSK_OFF_T
xfs_make_data_run_extent_index(TSK_FS_INFO * fs_info,
    TSK_FS_ATTR * fs_attr, TSK_FS_ATTR * fs_attr_extent,
    TSK_DADDR_T idx_block)
{
    ext2fs_extent_header *header;
    TSK_FS_ATTR_RUN *data_run;
    uint8_t *buf;
    ssize_t cnt;
    unsigned int i;

    /* first, read the block specified by the parameter */
    int fs_blocksize = fs_info->block_size;
    if ((buf = (uint8_t *) tsk_malloc(fs_blocksize)) == NULL) {
        return 1;
    }

    cnt =
        tsk_fs_read_block(fs_info, idx_block, (char *) buf, fs_blocksize);
    if (cnt != fs_blocksize) {
        if (cnt >= 0) {
            tsk_error_reset();
            tsk_error_set_errno(TSK_ERR_FS_READ);
        }
        tsk_error_set_errstr("xfs_make_data_run_extent_index: Block %"
            PRIuDADDR, idx_block);
        free(buf);
        return 1;
    }
    header = (ext2fs_extent_header *) buf;

    /* add it to the extent attribute */
    if (tsk_getu16(fs_info->endian, header->eh_magic) != 0xF30A) {
        tsk_error_set_errno(TSK_ERR_FS_INODE_COR);
        tsk_error_set_errstr
            ("xfs_make_data_run_extent_index: extent header magic valid incorrect!");
        free(buf);
        return 1;
    }

    data_run = tsk_fs_attr_run_alloc();
    if (data_run == NULL) {
        free(buf);
        return 1;
    }
    data_run->addr = idx_block;
    data_run->len = fs_blocksize;

    if (tsk_fs_attr_add_run(fs_info, fs_attr_extent, data_run)) {
        tsk_fs_attr_run_free(data_run);
        free(buf);
        return 1;
    }

    /* process leaf nodes */
    if (tsk_getu16(fs_info->endian, header->eh_depth) == 0) {
        ext2fs_extent *extents = (ext2fs_extent *) (header + 1);
        for (i = 0; i < tsk_getu16(fs_info->endian, header->eh_entries);
            i++) {
            ext2fs_extent extent = extents[i];
            if (ext2fs_make_data_run_extent(fs_info, fs_attr, &extent)) {
                free(buf);
                return 1;
            }
        }
    }
    /* recurse on interior nodes */
    else {
        ext2fs_extent_idx *indices = (ext2fs_extent_idx *) (header + 1);
        for (i = 0; i < tsk_getu16(fs_info->endian, header->eh_entries);
            i++) {
            ext2fs_extent_idx *index = &indices[i];
            TSK_DADDR_T child_block =
                (((uint32_t) tsk_getu16(fs_info->endian,
                        index->ei_leaf_hi)) << 16) | tsk_getu32(fs_info->
                endian, index->ei_leaf_lo);
            if (ext2fs_make_data_run_extent_index(fs_info, fs_attr,
                    fs_attr_extent, child_block)) {
                free(buf);
                return 1;
            }
        }
    }

    free(buf);
    return 0;
}

/** \internal
 * Get the number of extent blocks rooted at the given extent_header.
 * The count does not include the extent header passed as a parameter.
 *
 * @return the number of extent blocks, or -1 on error.
 */
static int32_t
xfs_extent_tree_index_count(TSK_FS_INFO * fs_info,
    TSK_FS_META * fs_meta, ext2fs_extent_header * header)
{
    int fs_blocksize = fs_info->block_size;
    ext2fs_extent_idx *indices;
    int count = 0;
    uint8_t *buf;
    int i;

    if (tsk_getu16(fs_info->endian, header->eh_magic) != 0xF30A) {
        tsk_error_set_errno(TSK_ERR_FS_INODE_COR);
        tsk_error_set_errstr
            ("ext2fs_load_attrs: extent header magic valid incorrect!");
        return -1;
    }

    if (tsk_getu16(fs_info->endian, header->eh_depth) == 0) {
        return 0;
    }

    if ((buf = (uint8_t *) tsk_malloc(fs_blocksize)) == NULL) {
        return -1;
    }

    indices = (ext2fs_extent_idx *) (header + 1);
    for (i = 0; i < tsk_getu16(fs_info->endian, header->eh_entries); i++) {
        ext2fs_extent_idx *index = &indices[i];
        TSK_DADDR_T block =
            (((uint32_t) tsk_getu16(fs_info->endian,
                    index->ei_leaf_hi)) << 16) | tsk_getu32(fs_info->
            endian, index->ei_leaf_lo);
        ssize_t cnt =
            tsk_fs_read_block(fs_info, block, (char *) buf, fs_blocksize);
        int ret;

        if (cnt != fs_blocksize) {
            if (cnt >= 0) {
                tsk_error_reset();
                tsk_error_set_errno(TSK_ERR_FS_READ);
            }
            tsk_error_set_errstr2("ext2fs_extent_tree_index_count: Block %"
                PRIuDADDR, block);
            return -1;
        }

        if ((ret =
                ext2fs_extent_tree_index_count(fs_info, fs_meta,
                    (ext2fs_extent_header *) buf)) < 0) {
            return -1;
        }
        count += ret;
        count++;
    }

    free(buf);
    return count;
}


/**
 * \internal
 * Loads attribute for XFS Extents-based storage method.
 * @param fs_file File system to analyze
 * @returns 0 on success, 1 otherwise
 */
static uint8_t
xfs_load_attrs_extents(TSK_FS_FILE *fs_file)
{
    TSK_FS_META *fs_meta = fs_file->meta;
    TSK_FS_INFO *fs_info = fs_file->fs_info;
    TSK_OFF_T length = 0;
    TSK_FS_ATTR *fs_attr;
    int i;
    ext2fs_extent *extents = NULL;
    ext2fs_extent_idx *indices = NULL;
    
    ext2fs_extent_header *header = (ext2fs_extent_header *) fs_meta->content_ptr;
    uint16_t num_entries = tsk_getu16(fs_info->endian, header->eh_entries);
    uint16_t depth = tsk_getu16(fs_info->endian, header->eh_depth);
    
    if (tsk_getu16(fs_info->endian, header->eh_magic) != 0xF30A) {
        tsk_error_set_errno(TSK_ERR_FS_INODE_COR);
        tsk_error_set_errstr
        ("xfs_load_attrs: extent header magic valid incorrect!");
        return 1;
    }
    
    if ((fs_meta->attr != NULL)
        && (fs_meta->attr_state == TSK_FS_META_ATTR_STUDIED)) {
        return 0;
    }
    else if (fs_meta->attr_state == TSK_FS_META_ATTR_ERROR) {
        return 1;
    }

    if (fs_meta->attr != NULL) {
        tsk_fs_attrlist_markunused(fs_meta->attr);
    }
    else {
        fs_meta->attr = tsk_fs_attrlist_alloc();
    }
    
    if (TSK_FS_TYPE_ISEXT(fs_info->ftype) == 0) {
        tsk_error_set_errno(TSK_ERR_FS_INODE_COR);
        tsk_error_set_errstr
        ("xfs_load_attr: Called with non-XFS file system: %x",
         fs_info->ftype);
        return 1;
    }
    
    length = roundup(fs_meta->size, fs_info->block_size);
    
    if ((fs_attr =
         tsk_fs_attrlist_getnew(fs_meta->attr,
                                TSK_FS_ATTR_NONRES)) == NULL) {
        return 1;
    }
    
    if (tsk_fs_attr_set_run(fs_file, fs_attr, NULL, NULL,
                            TSK_FS_ATTR_TYPE_DEFAULT, TSK_FS_ATTR_ID_DEFAULT,
                            fs_meta->size, fs_meta->size, length, 0, 0)) {
        return 1;
    }
    
    if (num_entries == 0) {
        fs_meta->attr_state = TSK_FS_META_ATTR_STUDIED;
        return 0;
    }
    
    if (depth == 0) {       /* leaf node */
        if (num_entries >
            (fs_info->block_size -
             sizeof(ext2fs_extent_header)) /
            sizeof(ext2fs_extent)) {
            tsk_error_set_errno(TSK_ERR_FS_INODE_COR);
            tsk_error_set_errstr
            ("xfs_load_attr: Inode reports too many extents");
            return 1;
        }
        
        extents = (ext2fs_extent *) (header + 1);
        for (i = 0; i < num_entries; i++) {
            ext2fs_extent extent = extents[i];
            if (ext2fs_make_data_run_extent(fs_info, fs_attr, &extent)) {
                return 1;
            }
        }
    }
    else {                  /* interior node */
        TSK_FS_ATTR *fs_attr_extent;
        int32_t extent_index_size;
        
        if (num_entries >
            (fs_info->block_size -
             sizeof(ext2fs_extent_header)) /
            sizeof(ext2fs_extent_idx)) {
            tsk_error_set_errno(TSK_ERR_FS_INODE_COR);
            tsk_error_set_errstr
            ("xfs_load_attr: Inode reports too many extent indices");
            return 1;
        }
        
        if ((fs_attr_extent =
             tsk_fs_attrlist_getnew(fs_meta->attr,
                                    TSK_FS_ATTR_NONRES)) == NULL) {
             return 1;
         }
        
        extent_index_size =
        ext2fs_extent_tree_index_count(fs_info, fs_meta, header);
        if (extent_index_size < 0) {
            return 1;
        }
        
        if (tsk_fs_attr_set_run(fs_file, fs_attr_extent, NULL, NULL,
                                TSK_FS_ATTR_TYPE_UNIX_EXTENT, TSK_FS_ATTR_ID_DEFAULT,
                                fs_info->block_size * extent_index_size,
                                fs_info->block_size * extent_index_size,
                                fs_info->block_size * extent_index_size, 0, 0)) {
            return 1;
        }
        
        indices = (ext2fs_extent_idx *) (header + 1);
        for (i = 0; i < num_entries; i++) {
            ext2fs_extent_idx *index = &indices[i];
            TSK_DADDR_T child_block =
            (((uint32_t) tsk_getu16(fs_info->endian,
                                    index->
                                    ei_leaf_hi)) << 16) | tsk_getu32(fs_info->
                                                                     endian, index->ei_leaf_lo);
            if (ext2fs_make_data_run_extent_index(fs_info, fs_attr,
                                                  fs_attr_extent, child_block)) {
                return 1;
            }
        }
    }
    
    fs_meta->attr_state = TSK_FS_META_ATTR_STUDIED;
    
    return 0;
}

/** \internal
 * Add the data runs and extents to the file attributes.
 *
 * @param fs_file File system to analyze
 * @returns 0 on success, 1 otherwise
 */
static uint8_t
xfs_load_attrs(TSK_FS_FILE * fs_file)
{
    if (fs_file->meta->content_type == TSK_FS_META_CONTENT_TYPE_EXT4_EXTENTS) {
        return ext4_load_attrs_extents(fs_file);
    }
    else {
        return tsk_fs_unix_make_data_run(fs_file);
    }
}

//inode walk
uint8_t xfs_inode_walk(TSK_FS_INFO * fs, TSK_INUM_T start, TSK_INUM_T end,
    TSK_FS_META_FLAG_ENUM flags, TSK_FS_META_WALK_CB cb, void *ptr)
{
    return -1;
}

//block walk
uint8_t xfs_block_walk(TSK_FS_INFO * fs, TSK_DADDR_T start, TSK_DADDR_T end, 
    TSK_FS_BLOCK_WALK_FLAG_ENUM flags, TSK_FS_BLOCK_WALK_CB cb, void *ptr)
{
    return -1;
}

//block_getflags
TSK_FS_BLOCK_FLAG_ENUM xfs_block_getflags(TSK_FS_INFO * a_fs, TSK_DADDR_T a_addr)
{
    int flags = 0;

    return flags;
}

//load_attrs
uint8_t xfs_load_attrs(TSK_FS_FILE *)
{
    return -1;
}

//file_add_meta
uint8_t xfs_inode_lookup(TSK_FS_INFO * fs, TSK_FS_FILE * fs_file, TSK_INUM_T addr)
{
    return -1;
}

//dir_open_meta
TSK_RETVAL_ENUM xfs_dir_open_meta(TSK_FS_INFO * fs, TSK_FS_DIR ** a_fs_dir, TSK_INUM_T inode)
{
    return TSK_OK;
}

//fsstat
uint8_t xfs_fsstat(TSK_FS_INFO * fs, FILE * hFile)
{
    return -1;
}

//fscheck
uint8_t xfs_fscheck(TSK_FS_INFO *, FILE *)
{
    return -1;
}

//istat
uint8_t xfs_istat(TSK_FS_INFO * fs, TSK_FS_ISTAT_FLAG_ENUM flags, FILE * hFile, TSK_INUM_T inum,
            TSK_DADDR_T numblock, int32_t sec_skew)
{
    return -1;
}

//close
void xfs_close(TSK_FS_INFO * fs)
{
    return;
}

//jblk_walk
uint8_t xfs_jblk_walk(TSK_FS_INFO *, TSK_DADDR_T, TSK_DADDR_T, int, TSK_FS_JBLK_WALK_CB, void *)
{
    return -1;
}

//jentry_walk
uint8_t xfs_jentry_walk(TSK_FS_INFO *, int, TSK_FS_JENTRY_WALK_CB, void *)
{
    return -1;
}

//jopen
uint8_t xfs_jopen(TSK_FS_INFO *, TSK_INUM_T)
{
    return -1;
}