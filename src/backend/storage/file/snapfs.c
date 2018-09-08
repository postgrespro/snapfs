SfsState * sfs_state;
sfs_snapshot_id sfs_current_snapshot;

int
sfs_msync(SnapshotMap * map)
{
	if (!enableFsync)
		return 0;
#ifdef WIN32
	return FlushViewOfFile(map, sizeof(SnapshotMap)) ? 0 : -1;
#else
	return msync(map, sizeof(SnapshotMap), MS_SYNC);
#endif
}

SnapshotMap *
sfs_mmap(int md)
{
	SnapshotMap    *map;

	if (ftruncate(md, sizeof(SnapshotMap)) != 0)
	{
		return (SnapshotMap *) MAP_FAILED;
	}

#ifdef WIN32
	{
		HANDLE		mh = CreateSnapshotMapping(_get_osfhandle(md), NULL, PAGE_READWRITE,
										   0, (DWORD) sizeof(SnapshotMap), NULL);

		if (mh == NULL)
			return (SnapshotMap *) MAP_FAILED;

		map = (SnapshotMap *) MapViewOfFile(mh, FILE_MAP_ALL_ACCESS, 0, 0, 0);
		CloseHandle(mh);
	}
	if (map == NULL)
		return (SnapshotMap *) MAP_FAILED;

#else
	map = (SnapshotMap *) mmap(NULL, sizeof(SnapshotMap), PROT_WRITE | PROT_READ, MAP_SHARED, md, 0);
#endif
	return map;
}

int
sfs_munmap(SnapshotMap * map)
{
#ifdef WIN32
	return UnmapViewOfFile(map) ? 0 : -1;
#else
	return munmap(map, sizeof(SnapshotMap));
#endif
}
/*
 * Safe read of file
 */
bool
sfs_read_file(int fd, void *data, uint32 size)
{
	uint32		offs = 0;

	do
	{
		int			rc = (int) read(fd, (char *) data + offs, size - offs);

		if (rc <= 0)
		{
			if (errno != EINTR)
				return false;
		}
		else
			offs += rc;
	} while (offs < size);

	return true;
}

/*
 * Safe write of file
 */
bool
sfs_write_file(int fd, void const *data, uint32 size)
{
	uint32		offs = 0;

	do
	{
		int			rc = (int) write(fd, (char const *) data + offs, size - offs);

		if (rc <= 0)
		{
			if (errno != EINTR)
				return false;
		}
		else
			offs += rc;
	} while (offs < size);

	return true;
}

void
sfs_set_backend_snapshot(sfs_snaphot_id snap_id)
{
	if (snap_id < sfs_state->oldest_snapshot || snap_id > sfs_state->recent_snapshot)
		elog(ERROR, "Invalid snapshot %d, existed snapshots %d..%d",
			 snap_id < sfs_state->oldest_snapshot, sfs_state->recent_snapshot);

	sfs_current_snapshot = snap_id;
	/* TODO: disable shared buffers and invalidate cache */
}

sfs_snapshot_id
sfs_make_snapshot(void)
{
	/* TODO: do checkpoint */
	/* TODO: wait completion of all active transactions */
	/* TODO: disable CLOG truncations */
	return ++sfs_state->recent_snapshot;
}

size_t
sfs_shmem_size(void)
{
	return sizeof(SfsState);
}

void
sfs_initialize(void)
{
	bool		found;
	sfs_state = (CfsState *) ShmemInitStruct("SFS Control", sfs_shmem_size(), &found);
	if (!found)
	{
		/* TODO: read from disk */
		sfs_state->oldest_snapshot = 0;
		sfs_state->recent_snapshot = -1;
	}
}
