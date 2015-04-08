#include <s_io.h>
#include <sdb.h>
#include <string.h>

int s_io_desc_init (SIO *io)
{
	if (!io || io->files)
		return S_FALSE;
	io->files = sdb_new0 ();
	return S_TRUE;
}

SIODesc *s_io_desc_new (SIOCbs *cbs, int fd, char *uri, int flags, void *data)
{
	SIODesc *desc = NULL;
	if (!cbs || !uri)
		return NULL;
	desc = R_NEW0 (SIODesc);
	desc->cbs = cbs;
	desc->fd = fd;
	desc->data = data;
	desc->flags = flags;
	desc->uri = strdup (uri);
	return desc;
}
