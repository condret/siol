#include <s_io.h>
#include <sdb.h>

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
	if (size && ((0xffffffffffffffff - size + 1) < at)) {									//second map
		s_io_map_new (io, desc->fd, desc->flags, 0xffffffffffffffff - at + 1, 0LL, size - (0xffffffffffffffff - at) - 1);	//split map into 2 maps if only 1 big map results into interger overflow
		size = 0xffffffffffffffff - at + 1;										//someone pls take a look at this confusing stuff
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
	if (!io)
		return 0;
	if (io->ff)
		memset (buf, 0xff, len);
	if (!io->desc || !(io->desc->flags & S_IO_READ) || !io->desc->cbs || !io->desc->cbs->read)				//check pointers and permissions
		return 0;
	s_io_desc_seek (io->desc, paddr, S_IO_SEEK_SET);
	return io->desc->cbs->read (io, io->desc, buf, len);
}

int s_io_pwrite_at (SIO *io, ut64 paddr, const ut8 *buf, int len)
{
	if (!io || !io->desc || !(io->desc->flags & S_IO_WRITE) || !io->desc->cbs || !io->desc->cbs->write)			//check pointers and permissions
		return 0;
	s_io_desc_seek (io->desc, paddr, S_IO_SEEK_SET);
	return io->desc->cbs->write (io, io->desc, buf, len);
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
void operate_on_itermap (SdbListIter *iter, SIO *io, ut64 vaddr, ut8 *buf, int len, int *(*op (SIO *io, ut64 addr, ut8 *buf, int len)))
{
	SIODesc *temp;
	SIOMap *map;
	ut64 vendaddr = vaddr + len - 1;
	if (!io || !len)
		return;
	if (!iter) {
		op (io, vaddr, buf, len);				//end of list
		return;
	}
	map = (SIOMap *)iter->data;
	while (!s_io_map_is_in_range (map, vaddr, vendaddr)) {		//search for next map or end of list
		iter = iter->p;
		if (!iter) {						//end of list
			op (io, vaddr, buf, len);			//pread/pwrite
			return;
		}
		map = (SIOMap *)iter->data;
	}
	if (map->from >= vaddr) {
		operate_on_itermap (iter->p, io, vaddr, buf, (int)(map->from - vaddr), op);
		buf = buf + (map->from - vaddr);
		vaddr = map->from;
		len = (int)(vendaddr - vaddr + 1);
		if (vendaddr <= map->to) {
			temp = io->desc;
			s_io_desc_use (io, map->fd);
			op (io, map->delta, buf, len);
			io->desc = temp;
		} else {
			temp = io->desc;
			s_io_desc_use (io, map->fd);
			op (io, map->delta, buf, len - (int)(vendaddr - map->to));
			io->desc = temp;
			vaddr = map->to + 1;
			buf = buf + (vendaddr - map->to);
			len = (int)(vendaddr - map->to);
			operate_on_itermap (iter->p, io, vaddr, buf, len, op);
		}
	} else {
		if (vendaddr <= map->to) {
			temp = io->desc;
			s_io_desc_use (io, map->fd);
			op (io, map->delta + (vaddr - map->from), buf, len);		//warning: may overflow in rare usecases
			io->desc = temp;
		} else {
			temp = io->desc;
			s_io_desc_use (io, map->fd);
			op (io, map->delta + (vaddr - map->from), buf, len - (int)(vendaddr - map->to));
			io->desc = temp;
			vaddr = map->to + 1;
			buf = buf + (vendaddr - map->to);
			len = (int)(vendaddr - map->to);
			operate_on_itermap (iter->p, io, vaddr, buf, len, op);
		}
	}
}
