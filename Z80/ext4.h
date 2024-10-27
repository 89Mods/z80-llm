#ifndef EXT4_H_
#define EXT4_H_

#define MINI_SUPERBLOCK_STRUCT

#define COMPAT_DIR_PREALLOC 0x001
#define COMPAT_IMAGIC_INODES 0x002
#define COMPAT_HAS_JOURNAL 0x004
#define COMPAT_EXT_ATTR 0x008
#define COMPAT_RESIZE_INODE 0x010
#define COMPAT_DIR_INDEX 0x020
#define COMPAT_LAZY_BG 0x040
#define COMPAT_EXCLUDE_INODE 0x080
#define COMPAT_EXCLUDE_BITMAP 0x100
#define COMPAT_SPARSE_SUPER2 0x200
#define COMPAT_FAST_COMMIT 0x400
#define COMPAT_ORPHAN_PRESENT 0x1000

#define INCOMPAT_COMPRESSION 0x0001
#define INCOMPAT_FILETYPE 0x0002
#define INCOMPAT_RECOVER 0x0004
#define INCOMPAT_JOURNAL_DEV 0x0008
#define INCOMPAT_META_BG 0x0010
#define INCOMPAT_EXTENTS 0x0040
#define INCOMPAT_64BIT 0x0080
#define INCOMPAT_MMP 0x0100
#define INCOMPAT_FLEX_BG 0x0200
#define INCOMPAT_EA_INODE 0x0400
#define INCOMPAT_DIRDATA 0x1000
#define INCOMPAT_CSUM_SEED 0x2000
#define INCOMPAT_LARGEDIR 0x4000
#define INCOMPAT_INLINE_DATA 0x8000
#define INCOMPAT_ENCRYPT 0x10000

#define RO_COMPAT_SPARSE_SUPER 0x0001
#define RO_COMPAT_LARGE_FILE 0x0002
#define RO_COMPAT_BTREE_DIR 0x0004
#define RO_COMPAT_HUGE_FILE 0x0008
#define RO_COMPAT_GDT_CSUM 0x0010
#define RO_COMPAT_DIR_NLINK 0x0020
#define RO_COMPAT_EXTRA_ISIZE 0x0040
#define RO_COMPAT_HAS_SNAPSHOT 0x0080
#define RO_COMPAT_QUOTA 0x0100
#define RO_COMPAT_BIGALLOC 0x0200
#define RO_COMPAT_METADATA_CSUM 0x0400
#define RO_COMPAT_REPLICA 0x0800
#define RO_COMPAT_READONLY 0x1000
#define RO_COMPAT_PROJECT 0x2000
#define RO_COMPAT_VERITY 0x8000
#define RO_COMPAT_ORPHAN_PRESENT 0x10000

#define EXT4_DEFM_DEBUG 0x0001
#define EXT4_DEFM_BSDGROUPS 0x0002
#define EXT4_DEFM_XATTR_USER 0x0004
#define EXT4_DEFM_ACL 0x0008
#define EXT4_DEFM_UID16 0x0010
#define EXT4_DEFM_JMODE_DATA 0x0020
#define EXT4_DEFM_JMODE_ORDERED 0x0040
#define EXT4_DEFM_JMODE_WBACK 0x0060
#define EXT4_DEFM_NOBARRIER 0x0100
#define EXT4_DEFM_BLOCK_VALIDITY 0x0200
#define EXT4_DEFM_DISCARD 0x0400
#define EXT4_DEFM_NODELALLOC 0x0800

#define EXT4_BG_INODE_UNINIT 0x1
#define EXT4_BG_BLOCK_UNINIT 0x2
#define EXT4_BG_INODE_ZEROED 0x4

#define S_IXOTH 0x01
#define S_IWOTH 0x02
#define S_IROTH 0x04
#define S_IXGRP 0x08
#define S_IWGRP 0x10
#define S_IRGRP 0x20
#define S_IXUSR 0x40
#define S_IWUSR 0x80
#define S_IRUSR 0x100
#define S_ISVTX 0x200
#define S_ISGID 0x400
#define S_ISUID 0x800
#define S_IFIFO 0x1000
#define S_IFCHR 0x2000
#define S_IFDIR 0x4000
#define S_IFBLK 0x6000
#define S_IFREG 0x8000
#define S_IFLNK 0xA000
#define S_IFSOCK 0xC000

#define EXT4_SECRM_FL 0x1
#define EXT4_UNRM_FL 0x2
#define EXT4_COMPR_FL 0x4
#define EXT4_SYNC_FL 0x8
#define EXT4_IMMUTABLE_FL 0x10
#define EXT4_APPEND_FL 0x20
#define EXT4_NODUMP_FL 0x40
#define EXT4_NOATIME_FL 0x80
#define EXT4_DIRTY_FL 0x100
#define EXT4_COMPRBLK_FL 0x200
#define EXT4_NOCOMPR_FL 0x400
#define EXT4_ENCRYPT_FL 0x800
#define EXT4_INDEX_FL 0x1000
#define EXT4_IMAGIC_FL 0x2000
#define EXT4_JOURNAL_DATA_FL 0x4000
#define EXT4_NOTAIL_FL 0x8000
#define EXT4_DIRSYNC_FL 0x10000
#define EXT4_TOPDIR_FL 0x20000
#define EXT4_HUGE_FILE_FL 0x40000
#define EXT4_EXTENTS_FL 0x80000
#define EXT4_VERITY_FL 0x100000
#define EXT4_EA_INODE_FL 0x200000
#define EXT4_EOFBLOCKS_FL 0x400000
#define EXT4_SNAPFILE_FL 0x01000000
#define EXT4_SNAPFILE_DELETED_FL 0x04000000
#define EXT4_SNAPFILE_SHRUNK_FL 0x08000000
#define EXT4_INLINE_DATA_FL 0x10000000
#define EXT4_PROJINHERIT_FL 0x20000000
#define EXT4_RESERVED_FL 0x80000000

//Internal errors
#define EXT4_OKAY 0
#define EXT4_LAST_EXTENT 1
#define EXT4_TOO_DEEP 2
#define EXT4_IO_ERR 3
#define EXT4_BLOCKS_TOO_BIG 4
#define EXT4_INVALID_MAGIC 5
#define EXT4_INCOMPAT 6
#define EXT4_INVALID_PARAMS 7
#define EXT4_CHECKSUM_BAD 8
#define EXT4_FILE_GAP 9
#define EXT4_EOF 10
#define EXT4_BAD_DIR_START 11
#define EXT4_BAD_ROOT_DIR 12
#define EXT4_BAD_DIR_TREE 13
#define EXT4_BAD_DIR_ENTRY 14
#define EXT4_FILE_NOT_FOUND 15
#define EXT4_FN_TOO_LONG 16

//Settings
#define EXT4_MAX_ETREE_DEPTH 4
#define EXT4_MAX_BLOCK_SIZE 4096
#define MAX_FN 28

typedef struct {
    uint32_t s_inodes_count;
    uint32_t s_blocks_count_lo;
    uint32_t s_r_blocks_count_lo;
    uint32_t s_free_blocks_count_lo;
    uint32_t s_free_inodes_count;
    uint32_t s_first_data_block;
    uint32_t s_log_block_size;
    uint32_t s_log_cluster_size;
    uint32_t s_blocks_per_group;
    uint32_t s_clusters_per_group;
    uint32_t s_inodes_per_group;
    uint32_t s_mtime;
    uint32_t s_wtime;
    uint16_t s_mnt_count;
    uint16_t s_max_mnt_count;
    uint16_t s_magic;
    uint16_t s_state;
    uint16_t s_errors;
    uint16_t s_minor_rev_level;
    uint32_t s_lastcheck;
    uint32_t s_checkinterval;
    uint32_t s_creator_os;
    uint32_t s_rev_level;
    uint16_t s_def_resuid;
    uint16_t s_def_resgid;
    //EXT4_DYNAMIC_REV only
    uint32_t s_first_ino;
    uint16_t s_inode_size;
    uint16_t s_block_group_nr;
    uint32_t s_feature_compat;
    uint32_t s_feature_incompat;
    uint32_t s_feature_ro_compat;
    uint8_t s_uuid[16];
    uint8_t s_volume_name[16];
    uint8_t s_last_mounted[64];
    uint32_t s_algorithm_usage_bitmap;
    //Only if EXT4_FEATURE_COMPAT_DIR_PREALLOC
    uint8_t s_prealloc_blocks;
    uint8_t s_prealloc_dir_blocks;
    uint16_t s_reserved_gdt_blocks;
    //Only if EXT4_FEATURE_COMPAT_HAS_JOURNAL
    uint8_t s_journal_uuid[16];
    uint32_t s_journal_inum;
    uint32_t s_journal_dev;
    uint32_t s_last_orphan;
    uint32_t s_hash_seed[4];
    uint8_t s_def_hash_version;
    uint8_t s_jnl_backup_type;
    uint16_t s_desc_size;
    uint32_t s_default_mount_opts;
    uint32_t s_first_meta_bg;
    uint32_t s_mkfs_time;
    uint32_t s_jnl_blocks[17];
    //Only if EXT4_FEATURE_COMPAT_64BIT
    uint32_t s_blocks_count_hi;
    uint32_t s_r_blocks_count_hi;
    uint32_t s_free_blocks_count_hi;
    uint16_t s_min_extra_isize;
    uint16_t s_want_extra_isize;
    uint32_t s_flags;
    uint16_t s_raid_stride;
    uint16_t s_mmp_interval;
    uint64_t s_mmp_block;
    uint32_t s_raid_stripe_width;
    uint8_t s_log_groups_per_flex;
    uint8_t s_checksum_type;
    uint16_t s_reserved_pad;
    uint64_t s_kbytes_written;
    uint32_t s_snapshot_inum;
    uint32_t s_snapshot_id;
    uint64_t s_snapshot_r_blocks_count;
    uint32_t s_snapshot_list;
    uint32_t s_error_count;
    uint32_t s_first_error_time;
    uint32_t s_first_error_ino;
    uint64_t s_first_error_block;
    uint8_t s_first_error_func[32];
    uint32_t s_first_error_line;
    uint32_t s_last_error_time;
    uint32_t s_last_error_ino;
    uint32_t s_last_error_line;
    uint64_t s_last_error_block;
    uint8_t s_last_error_func[32];
    uint8_t s_mount_opts[64];
    uint32_t s_usr_quota_inum;
    uint32_t s_grp_quota_inum;
    uint32_t s_overhead_blocks;
    uint32_t s_backup_bgs[2];
    uint8_t s_encrypt_algos[4];
    uint8_t s_encrypt_pw_salt[16];
    uint32_t s_lpf_ino;
    uint32_t s_prj_quota_inum;
    uint32_t s_checksum_seed;
    uint8_t s_wtime_hi;
    uint8_t s_mtime_hi;
    uint8_t s_mkfs_time_hi;
    uint8_t s_lastcheck_hi;
    uint8_t s_first_error_time_hi;
    uint8_t s_last_error_time_hi;
    uint8_t s_pad[2];
    uint16_t s_encoding;
    uint16_t s_encoding_flags;
    uint32_t s_orphan_file_inum;
    uint32_t s_reserved[94];
    uint32_t s_checksum;
} ext4_super_block;

#ifdef MINI_SUPERBLOCK_STRUCT
typedef struct {
    uint32_t s_first_data_block;
    uint32_t s_inodes_per_group;
    uint16_t s_inode_size;
    uint16_t s_desc_size;
    uint32_t s_log_block_size;
    uint16_t s_magic;
    uint32_t s_blocks_per_group;
    uint32_t s_feature_compat;
    uint32_t s_feature_incompat;
    uint32_t s_feature_ro_compat;
    uint8_t s_def_hash_version;
    uint8_t s_log_groups_per_flex;
#ifdef CHECKSUM
    uint8_t s_uuid[16];
#endif
} ext4_super_block_mini;
#endif

typedef struct {
    uint32_t bg_block_bitmap_lo;
    uint32_t bg_inode_bitmap_lo;
    uint32_t bg_inode_table_lo;
    uint16_t bg_free_blocks_count_lo;
    uint16_t bg_free_inodes_count_lo;
    uint16_t bg_used_dirs_count_lo;
    uint16_t bg_flags;
    uint32_t bg_exclude_bitmap_lo;
    uint16_t bg_block_bitmap_csum_lo;
    uint16_t bg_inode_bitmap_csum_lo;
    uint16_t bg_itable_unused_lo;
    uint16_t bg_checksum;
    //Only if EXT4_FEATURE_COMPAT_64BIT
    uint32_t bg_block_bitmap_hi;
    uint32_t bg_inode_bitmap_hi;
    uint32_t bg_inode_table_hi;
    uint16_t bg_free_blocks_count_hi;
    uint16_t bg_free_inodes_count_hi;
    uint16_t bg_used_dirs_count_hi;
    uint16_t bg_itable_unused_hi;
    uint32_t bg_exclude_bitmap_hi;
    uint16_t bg_block_bitmap_csum_hi;
    uint16_t bg_inode_bitmap_csum_hi;
    uint32_t bg_reserved;
} ext4_group_desc_raw;

typedef struct {
    uint64_t bg_block_bitmap;
    uint64_t bg_inode_bitmap;
    uint64_t bg_inode_table;
    uint32_t bg_free_blocks_count;
    uint32_t bg_free_inodes_count;
    uint32_t bg_used_dirs_count;
    uint16_t bg_flags;
    uint64_t bg_exclude_bitmap;
    uint32_t bg_block_bitmap_csum;
    uint32_t bg_inode_bitmap_csum;
    uint32_t bg_itable_unused;
    uint16_t bg_checksum;
    uint8_t checksum_valid;
} ext4_group_desc;

typedef struct {
    uint16_t i_mode;
    uint16_t i_uid;
    uint32_t i_size_lo;
    int32_t i_atime;
    int32_t i_ctime;
    int32_t i_mtime;
    int32_t i_dtime;
    uint16_t i_gid;
    uint16_t i_links_count;
    uint32_t i_blocks_lo;
    uint32_t i_flags;
    uint32_t i_osd1;
    uint8_t i_block[60];
    uint32_t i_generation;
    uint32_t i_file_acl_lo;
    uint32_t i_size_high;
    uint32_t i_obso_faddr;
    //Going with the Linux interpretation of osd2
    uint16_t i_blocks_high;
    uint16_t i_file_acl_high;
    uint16_t i_uid_high;
    uint16_t i_gid_high;
    uint16_t i_checksum_lo;
    uint16_t i_reserved;
    //End of osd2
    uint16_t i_extra_isize; //Size of the inode beyond the traditional 128 bytes, including this field
    uint16_t i_checksum_hi;
    uint32_t i_ctime_extra;
    uint32_t i_mtime_extra;
    uint32_t i_atime_extra;
    int32_t i_crtime;
    uint32_t i_crtime_extra;
    uint32_t i_version_hi;
    uint32_t i_projid;
} ext4_inode_raw;

typedef struct {
    uint16_t i_mode;
    uint32_t i_uid;
    uint64_t i_size;
    int64_t i_atime;
    int64_t i_ctime;
    int64_t i_mtime;
    int32_t i_dtime;
    uint32_t i_gid;
    uint16_t i_links_count;
    uint64_t i_blocks;
    uint32_t i_flags;
    uint8_t i_block[60];
    uint32_t i_generation;
    uint64_t i_file_acl;
    int64_t i_crtime;
    uint64_t i_version;
    uint32_t i_projid;
    uint32_t i_osd1;
    uint16_t i_extra_isize;
    uint8_t checksum_valid;
    uint64_t inode_num;
} ext4_inode;

typedef struct {
    uint16_t eh_magic;
    uint16_t eh_entries;
    uint16_t eh_max;
    uint16_t eh_depth;
    uint32_t eh_generation;
} ext4_extent_header;

typedef struct {
    uint32_t ei_block;
    uint32_t ei_leaf_lo;
    uint16_t ei_leaf_hi;
    uint16_t ei_unused;
} ext4_extent_idx;

typedef struct {
    uint32_t ee_block;
    uint16_t ee_len;
    uint16_t ee_start_hi;
    uint32_t ee_start_lo;
} ext4_extent;

typedef struct {
    uint32_t eb_checksum; // = crc32c(uuid+inum+igeneration+extentblock)
} ext4_extent_tail;

typedef struct {
	uint32_t inode;
	uint16_t rec_len;
	uint8_t name_len;
	uint8_t file_type;
} ext4_dir_entry_2;

typedef struct {
    uint64_t block; //Block of the ext4_extent_header (always at start of block), or 0xFFFFFFFFFFFFFFFF if inode.i_block
    uint16_t curr_entry; //Which entry following the header this is
} ext4_etree_entry;

typedef struct {
	uint32_t reserved_zero;
	uint8_t hash_version;
	uint8_t info_length;
	uint8_t indirect_levels;
	uint8_t unused_flags;
	uint16_t limit;
	uint16_t count;
	uint32_t block;
} ext4_dx_root;

typedef struct {
    uint64_t inode_num; //Which inode
    uint32_t inode_flags;
    uint64_t position; //Which byte position within the file (read pointer)
    uint64_t limit; //File size
    ext4_extent curr_extent; //The extent currently being traversed
    uint8_t curr_depth;
    ext4_etree_entry path[EXT4_MAX_ETREE_DEPTH]; //Represents a path through the extent tree from root node (index 0) to the current node (index curr_depth)
} ext4_FIL;

typedef struct {
	ext4_FIL fhandle;
	uint32_t dot_inode;
	uint32_t dotdot_inode;
	uint64_t tree_start;
} ext4_DIR;

uint8_t ext4_read_inode(uint64_t inode_num, ext4_inode *ret_inode);
int ext4_mount(uint32_t lba_offset);
uint8_t ext4_open_read(uint64_t inode_num, ext4_FIL* fhandle);
uint8_t ext4_read(ext4_FIL* fhandle, uint8_t* buff, uint32_t count, uint32_t* read);
uint8_t ext4_seek(ext4_FIL* fhandle, uint64_t position);
uint8_t ext4_open_dir(uint64_t inode_num, ext4_DIR *dhandle);
uint8_t ext4_dir_next(ext4_DIR *dhandle, uint64_t *inode_num, char *name, uint16_t nbuff_size, uint8_t *file_type);
uint8_t ext4_find_file(char *path, uint64_t *inode_num, uint8_t *file_type);
#ifdef SUPPORT_SYMLINKS
uint8_t ext4_resolve_symlink(uint64_t dir_inode, uint64_t symlink_inode, uint64_t *inode_num, uint8_t *file_type);
#endif
void ext4_test_list_dir(uint32_t inode);

#endif
