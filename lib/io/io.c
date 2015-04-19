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
	if ((0xffffffffffffffff-size) < at)
		size = 0xffffffffffffffff - at;
	s_io_map_new (io, desc->fd, desc->flags, 0LL, at, size);
	return desc;
}
