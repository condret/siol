#include <s_io.h>
#include <sdb.h>
#include <stdio.h>

void operate_on_itermap (SdbListIter *iter, SIO *io, ut64 vaddr, ut8 *buf, int len, int match_flg, int (op (SIO *io, ut64 addr, ut8 *buf, int len)));

SIO *s_io_new ()
{
	SIO *ret = R_NEW0 (SIO);
	return s_io_init (ret);
}

SIO *s_io_init (SIO *io)
{
	if (!io)
		return NULL;
	s_io_desc_init (io);
	s_io_map_init (io);
	return io;
}

SIODesc *s_io_open_nomap (SIO *io, SIOCbs *cbs, char *uri, int flags, int mode)
{
	SIODesc *desc;
	if (!io || !io->files || !cbs || !cbs->open || !cbs->close || !uri)
		return NULL;
	desc = cbs->open (io, uri, flags, mode);
	if (!desc)
		return NULL;
	if (!desc->cbs)						//for none static callbacks, those that cannot use s_io_desc_new
		desc->cbs = cbs;
	s_io_desc_add (io, desc);
	if (io->autofd || !io->desc)				//set desc as current if autofd or io->desc==NULL
		io->desc = desc;
	return desc;
}

SIODesc *s_io_open (SIO *io, SIOCbs *cbs, char *uri, int flags, int mode)
{
	SIODesc *desc;
	if (!io || !io->maps)
		return NULL;
	desc = s_io_open_nomap (io, cbs, uri, flags, mode);
	if (!desc)
		return NULL;
	s_io_map_new (io, desc->fd, desc->flags, 0LL, 0LL, s_io_desc_size (desc));
	return desc;
}

SIODesc *s_io_open_at (SIO *io, SIOCbs *cbs, char *uri, int flags, int mode, ut64 at)
{
	SIODesc *desc;
	ut64 size;
	if (!io || !io->maps)
		return NULL;
	desc = s_io_open_nomap (io, cbs, uri, flags, mode);
	if (!desc)
		 return NULL;
	size = s_io_desc_size (desc);
	if (size && ((UT64_MAX - size + 1) < at)) {									//second map
		s_io_map_new (io, desc->fd, desc->flags, UT64_MAX - at + 1, 0LL, size - (UT64_MAX - at) - 1);	//split map into 2 maps if only 1 big map results into interger overflow
		size = UT64_MAX - at + 1;										//someone pls take a look at this confusing stuff
	}
	s_io_map_new (io, desc->fd, desc->flags, 0LL, at, size);								//first map
	return desc;
}

int s_io_close (SIO *io, int fd)
{
	SIODesc *desc = s_io_desc_get (io, fd);
	if (!desc || !desc->cbs || !desc->cbs->close)										//check for cb
		return S_FALSE;
	if (!desc->cbs->close (desc))												//close fd
		return S_FALSE;
	s_io_desc_del (io, fd);													//remove entry from sdb-instance and free the desc-struct
	s_io_map_cleanup (io);													//remove all dead maps
	return S_TRUE;
}

int s_io_pread_at (SIO *io, ut64 paddr, ut8 *buf, int len)
{
	printf ("s_io_pread: called\n");
	if (!io || !buf) {
		printf ("s_io_pread: io = %p ; buf = %p\n", io, buf);
		return 0;
	}
	printf ("s_io_pread: io->ff = %d ; paddr = 0x%llx ; len = %d\n", io->ff, paddr, len);
	if (io->ff)
		memset (buf, 0xff, len);
	if (!io->desc || !(io->desc->flags & S_IO_READ) || !io->desc->cbs || !io->desc->cbs->read || !len)			//check pointers and permissions
		return 0;
	printf ("s_io_pread: io->desc->fd = %d\n", io->desc->fd);
	s_io_desc_seek (io->desc, paddr, S_IO_SEEK_SET);
	return io->desc->cbs->read (io, io->desc, buf, len);
}

int s_io_pwrite_at (SIO *io, ut64 paddr, ut8 *buf, int len)
{
	if (!io || !buf || !io->desc || !(io->desc->flags & S_IO_WRITE) || !io->desc->cbs || !io->desc->cbs->write || !len)	//check pointers and permissions
		return 0;
	s_io_desc_seek (io->desc, paddr, S_IO_SEEK_SET);
	return io->desc->cbs->write (io, io->desc, buf, len);
}

int s_io_vread_at (SIO *io, ut64 vaddr, ut8 *buf, int len)
{
	printf ("s_io_vread: called\n");
	if (!io || !buf) {
		printf ("s_io_vread: io = %p ; buf = %p\n", io, buf);
		return S_FALSE;
	}
	printf ("s_io_vread: io->maps = %p ; vaddr = 0x%llx ; len %d\n", io->maps, vaddr, len);
	if (!len)
		return S_TRUE;
	printf ("io->maps->tail %p\n", io->maps->tail);
	s_io_map_cleanup (io);
	printf ("io->maps->tail %p\n", io->maps->tail);
	if (!io->maps)
		return s_io_pread_at (io, vaddr, buf, len);
	operate_on_itermap (io->maps->tail, io, vaddr, buf, len, S_IO_READ, s_io_pread_at);
	return S_TRUE;
}

int s_io_vwrite_at (SIO *io, ut64 vaddr, ut8 *buf, int len)
{
	if (!io || !buf)
		return S_FALSE;
	if (!len)
		return S_TRUE;
	s_io_map_cleanup (io);
	if (!io->maps)
		return s_io_pwrite_at (io, vaddr, buf, len);
	operate_on_itermap (io->maps->tail, io, vaddr, buf, len, S_IO_WRITE, s_io_pwrite_at);
	return S_TRUE;
}

int s_io_read_at (SIO *io, ut64 addr, ut8 *buf, int len)
{
	printf ("s_io_read_at: called\n");
	if (!io || !buf || !len) {
		printf ("s_io_read_at: io = %p ; buf = %p ; len = %d\n", io, buf, len);
		return 0;
	}
	printf ("s_io_read_at: io->va = %d ; addr = 0x%llx ; len = %d\n", io->va, addr, len);
	if (io->va)
		return s_io_vread_at (io, addr, buf, len);
	return s_io_pread_at (io, addr, buf, len);
}

int s_io_write_at (SIO *io, ut64 addr, ut8 *buf, int len)
{
	if (!io || !buf || !len)
		return 0;
	if (io->va)
		return s_io_vwrite_at (io, addr, buf, len);
	return s_io_pwrite_at (io, addr, buf, len);
}

//remove all descs and maps
int s_io_fini (SIO *io)
{
	if (!io)
		return S_FALSE;
	s_io_desc_fini (io);
	s_io_map_fini (io);
	return S_TRUE;
}

void s_io_free (SIO *io)
{
	s_io_fini (io);
	free (io);
}

//not public api
void operate_on_itermap (SdbListIter *iter, SIO *io, ut64 vaddr, ut8 *buf, int len, int match_flg, int (op (SIO *io, ut64 addr, ut8 *buf, int len)))
{
	SIODesc *temp;
	SIOMap *map;
	ut64 vendaddr;
	printf ("operate_on_itermap: called ; "
		"io = %p ; buf = %p ; vaddr = 0x%llx ; len = %d\n", io, buf, vaddr, len);
	if (!io || !len || !buf)
		return;
	if (!iter) {
		printf ("operate_on_itermap: call op @ 1\n");
		op (io, vaddr, buf, len);				//end of list
		return;
	}
	if ((UT64_MAX - len + 1) < vaddr) {				//this block is not that much elegant
		int nlen;						//needed for edge-cases
		vendaddr = UT64_MAX;					//add a test for this block
		nlen = (int)(vendaddr - vaddr + 1);
		operate_on_itermap (iter, io, 0LL, buf + nlen, len - nlen, match_flg, op);
	} else	vendaddr = vaddr + len - 1;
	map = (SIOMap *)iter->data;
	while (!s_io_map_is_in_range (map, vaddr, vendaddr)) {		//search for next map or end of list
		iter = iter->p;
		if (!iter) {						//end of list
			printf ("operate_on_itermap: call op @ 2\n");
			op (io, vaddr, buf, len);			//pread/pwrite
			return;
		}
		map = (SIOMap *)iter->data;
	}
	if (map->from >= vaddr) {
		operate_on_itermap (iter->p, io, vaddr, buf, (int)(map->from - vaddr), match_flg, op);
		buf = buf + (map->from - vaddr);
		vaddr = map->from;
		len = (int)(vendaddr - vaddr + 1);
		if (vendaddr <= map->to) {
			if ((map->flags & match_flg) == match_flg) {
				temp = io->desc;
				s_io_desc_use (io, map->fd);
				printf ("operate_on_itermap: call op @ 3\n");
				op (io, map->delta, buf, len);
				io->desc = temp;
			}
		} else {
			if ((map->flags & match_flg) == match_flg) {
				temp = io->desc;
				s_io_desc_use (io, map->fd);
				printf ("operate_on_itermap: call op @ 4\n");
				op (io, map->delta, buf, len - (int)(vendaddr - map->to));
				io->desc = temp;
			}
			vaddr = map->to + 1;
			buf = buf + (len - (int)(vendaddr - map->to));
			len = (int)(vendaddr - map->to);
			operate_on_itermap (iter->p, io, vaddr, buf, len, match_flg, op);
		}
	} else {
		if (vendaddr <= map->to) {
			if ((map->flags & match_flg) == match_flg) {
				temp = io->desc;
				s_io_desc_use (io, map->fd);
				printf ("operate_on_itermap: call op @ 5\n");
				op (io, map->delta + (vaddr - map->from), buf, len);		//warning: may overflow in rare usecases
				io->desc = temp;
			}
		} else {
			if ((map->flags & match_flg) == match_flg) {
				temp = io->desc;
				s_io_desc_use (io, map->fd);
				printf ("operate_on_itermap: call op @ 6\n");
				op (io, map->delta + (vaddr - map->from), buf, len - (int)(vendaddr - map->to));
				io->desc = temp;
			}
			vaddr = map->to + 1;
			buf = buf + (len - (int)(vendaddr - map->to));
			len = (int)(vendaddr - map->to);
			operate_on_itermap (iter->p, io, vaddr, buf, len, match_flg, op);
		}
	}
}
