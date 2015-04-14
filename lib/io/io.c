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

SIODesc *s_io_open_nomap (SIO *io, SIOCbs *cbs, char *uri, int flags)
{
	SIODesc *desc;
	if (!io || !io->files || !cbs || !cbs->open || !cbs->close || !uri)
		return NULL;
	desc = cbs->open (io, uri, flags, 0);			//mode should do what?
	if (!desc)
		return NULL;
	if (!desc->cbs)						//for none static callbacks, those that cannot use s_io_desc_new
		desc->cbs = cbs;
	s_io_desc_add (io, desc);
	return desc;
}
