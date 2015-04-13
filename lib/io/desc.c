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
	desc->uri = strdup (uri);			//because the uri-arg may live on the stack
	return desc;
}

int s_io_desc_add (SIO *io, SIODesc *desc)
{
	char s[64];
	if (!io || !io->files || !desc)
		return S_FALSE;
	sdb_itoa ((ut64)desc->fd, s, 16);
	if (sdb_num_exists (io->files, s))		//check if fd already exists in db
		return S_FALSE;
	sdb_num_set (io->files, s, (ut64)desc, 0);
	return sdb_num_exists (io->files, s);		//check if storage worked
}
