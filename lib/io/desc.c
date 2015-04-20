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

void s_io_desc_free (SIODesc *desc)
{
	if (desc) {
		free (desc->uri);
		free (desc->cbs);
	}
	free (desc);
}

int s_io_desc_add (SIO *io, SIODesc *desc)
{
	char s[64];
	if (!io || !io->files || !desc)
		return S_FALSE;
	sdb_itoa ((ut64)desc->fd, s, 10);
	if (sdb_num_exists (io->files, s))		//check if fd already exists in db
		return S_FALSE;
	sdb_num_set (io->files, s, (ut64)desc, 0);
	return sdb_num_exists (io->files, s);		//check if storage worked
}

int s_io_desc_del (SIO *io, int fd)
{
	char s[64];
	if (!io || !io->files)
		return S_FALSE;
	sdb_itoa ((ut64)fd, s, 10);
	s_io_desc_free ((SIODesc *)sdb_num_get (io->files, s, NULL));
	if ((ut64)io->desc == sdb_num_get (io->files, s, NULL))
		io->desc = NULL;					//prevent evil segfaults
	return sdb_unset (io->files, s, 0);
}

SIODesc *s_io_desc_get (SIO *io, int fd)
{
	char s[64];
	if (!io || !io->files)
		return NULL;
	sdb_itoa ((ut64)fd, s, 10);
	return (SIODesc *)sdb_num_get (io->files, s, NULL);
}

int s_io_desc_use (SIO *io, int fd)
{
	SIODesc *desc;
	if (!(desc = s_io_desc_get (io, fd)))
		return S_FALSE;
	io->desc = desc;
	return S_TRUE;
}

ut64 s_io_desc_seek (SIODesc *desc, ut64 offset, int whence)
{
	if (!desc || !desc->cbs || !desc->cbs->lseek)
		return (ut64)-1;
	return desc->cbs->lseek (desc->io, desc, offset, whence);
}

ut64 s_io_desc_size (SIODesc *desc)
{
	ut64 off, ret;
	if (desc || !desc->cbs || !desc->cbs->lseek)
		return 0LL;
	off = desc->cbs->lseek (desc->io, desc, 0LL, S_IO_SEEK_CUR);
	ret = desc->cbs->lseek (desc->io, desc, 0LL, S_IO_SEEK_END);
	desc->cbs->lseek (desc->io, desc, off, S_IO_SEEK_CUR);			//what to do if that seek fails?
	return ret;
}
