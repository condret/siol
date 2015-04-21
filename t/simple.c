#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <s_io.h>

SIODesc *__open_cb (SIO *io, const char *uri, int flags, int mode);
int __read_cb (SIO *io, SIODesc *desc, ut8 *buf, int len);
ut64 __lseek_cb (SIO *io, SIODesc *desc, ut64 offset, int whence);
int __close_cb (SIODesc *desc);

struct s_io_callbacks_t test_cbs = {
	.open = __open_cb,
	.read = __read_cb,
	.lseek= __lseek_cb,
	.write= NULL,
	.close= __close_cb,
};

SIODesc *__open_cb (SIO *io, const char *uri, int flags, int mode)
{
	int fflags;
	int fd;
	if (!uri)
		return NULL;
	switch (flags) {
		case S_IO_READ:
			fflags = O_RDONLY;
			break;
		case S_IO_WRITE:
			fflags = O_WRONLY;
			break;
		case S_IO_RW:
			fflags = O_RDWR;
			break;
		default:
			return NULL;
	}
	fd = open (uri, fflags);
	return s_io_desc_new (&test_cbs, fd, uri, flags, NULL);
}

int __read_cb (SIO *io, SIODesc *desc, ut8 *buf, int len)
{
	if (!desc)
		return -1;
	return read (desc->fd, buf, (size_t)len);
}

ut64 __lseek_cb (SIO *io, SIODesc *desc, ut64 offset, int whence)
{
	if (!desc)
		return 0;
	switch (whence) {
		case S_IO_SEEK_SET:
			return lseek (desc->fd, offset, SEEK_SET);
		case S_IO_SEEK_CUR:
			return lseek (desc->fd, offset, SEEK_CUR);
	}
	return lseek (desc->fd, offset, SEEK_END);
}

int __close_cb (SIODesc *desc)
{
	if (!desc)
		return S_FALSE;
	close (desc->fd);
	return S_TRUE;
}

int main ()
{
	ut8 buf[6];
	SIO *io = s_io_new ();
	io->ff = io->autofd = S_TRUE;
	printf ("io got initialized\n");
	s_io_open_nomap (io, &test_cbs, "foo", S_IO_READ, 0);
	printf ("opened file foo\n");
	s_io_pread_at (io, 0LL, buf, 3);
	buf[3] = (ut8)0;
	printf ("read 3 chars: %s\n", (char *)buf);
	s_io_free (io);
	return S_TRUE;
}