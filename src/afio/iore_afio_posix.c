/*
 * iore_afio_posix.c
 *
 * Author: Camilo <eduardo.camilo@posgrad.ufsc.br>
 */

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 500
#endif

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

#include "iore_afio.h"
#include "iore_error.h"
#include "iore_util.h"
#include "iore_workload.h"
#include "iore_ctx.h"

/*** PROTOTYPES **************************************************************/

int
posix_create (iore_file_t *, const iore_test_t *);
int
posix_open (iore_file_t *, const iore_test_t *);
ssize_t
posix_write_oset (iore_file_t, const void *, const off_t *, const iore_test_t *);
ssize_t
posix_read_oset (iore_file_t, void *, const off_t *, const iore_test_t *);
ssize_t
posix_write_dset (iore_file_t, const void *, const iore_test_t *);
ssize_t
posix_read_dset (iore_file_t, void *, const iore_test_t *);
int
posix_close (iore_file_t *);
int
posix_remove (iore_file_t);

/*** VARIABLES ***************************************************************/

const iore_afio_vtable_t afio_posix =
  { posix_create, posix_open, posix_write_oset, posix_read_oset,
      posix_write_dset, posix_read_dset, posix_close, posix_remove };

/*** FUNCTIONS ***************************************************************/

int
posix_create (iore_file_t *file, const iore_test_t *test)
{
  assert(file);
  assert(test);

  int rerr = IORE_SUCCESS;

  int fd;
  int oflag = O_CREAT | O_WRONLY;
  mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

  fd = open (file->name, oflag, mode);
  if (fd >= 0)
    file->hdle.fint = fd;
  else
    rerr = fd;

  return rerr;
} /* posix_create () */

int
posix_open (iore_file_t *file, const iore_test_t *test)
{
  assert(file);
  assert(test);

  int rerr = 0;

  int fd;
  int oflag = O_RDONLY;

  fd = open (file->name, oflag);
  if (fd >= 0)
    file->hdle.fint = fd;
  else
    rerr = fd;

  return rerr;
} /* posix_open () */

ssize_t
posix_write_oset (iore_file_t file, const void *buf, const off_t *offs,
		  const iore_test_t *test)
{
  assert(buf);
  assert(offs);
  assert(test);

  ssize_t nbytes = 0;

  int fd = file.hdle.fint;
  size_t file_size = test->wkld.u.oset._file_size;
  size_t remaining = test->wkld.u.oset.my_data_size;
  size_t max_req_size = test->wkld.u.oset.my_req_size;
  size_t req_size;
  ssize_t xferd;

  bool seek_rw_single_op = strtob (
      dict_get (&test->afio.params, AFIO_PARAM_SEEK_RW_SINGLE_OP));
  if (seek_rw_single_op)
    {
      while (remaining && nbytes >= 0)
	{
	  req_size = (file_size - *offs);
	  if (req_size > max_req_size)
	    req_size = max_req_size;
	  if (req_size > remaining)
	    req_size = remaining;
	  xferd = pwrite (fd, buf, req_size, *offs);
	  if (xferd < (ssize_t) req_size)
	    nbytes = -1;
	  else
	    {
	      nbytes += xferd;
	      if (test->write_flush_per_req)
		fsync (fd);
	    }
	  remaining -= req_size;
	  offs++;
	}
    }
  else /* not seek_rw_single_op */
    {
      while (remaining && nbytes >= 0)
	{
	  if (lseek (fd, *offs, SEEK_SET) < 0)
	    nbytes = -1;
	  else
	    {
	      req_size = (file_size - *offs);
	      if (req_size > max_req_size)
		req_size = max_req_size;
	      if (req_size > remaining)
		req_size = remaining;
	      xferd = write (fd, buf, req_size);
	      if (xferd < (ssize_t) req_size)
		nbytes = -1;
	      else
		{
		  nbytes += xferd;
		  if (test->write_flush_per_req)
		    fsync (fd);
		}
	    }
	  remaining -= req_size;
	  offs++;
	}
    }

  if (nbytes > 0 && test->write_flush)
    fsync (fd);

  return nbytes;
} /* posix_write_oset () */

ssize_t
posix_read_oset (iore_file_t file, void *buf, const off_t *offs,
		 const iore_test_t *test)
{
  assert(buf);
  assert(offs);
  assert(test);

  ssize_t nbytes = 0;

  int fd = file.hdle.fint;
  size_t file_size = test->wkld.u.oset._file_size;
  size_t remaining = test->wkld.u.oset.my_data_size;
  size_t max_req_size = test->wkld.u.oset.my_req_size;
  size_t req_size;
  ssize_t xferd;

  bool seek_rw_single_op = strtob (
      dict_get (&test->afio.params, AFIO_PARAM_SEEK_RW_SINGLE_OP));
  if (seek_rw_single_op)
    {
      while (remaining && nbytes >= 0)
	{
	  req_size = (file_size - *offs);
	  if (req_size > max_req_size)
	    req_size = max_req_size;
	  if (req_size > remaining)
	    req_size = remaining;
	  xferd = pread (fd, buf, req_size, *offs);
	  if (xferd < (ssize_t) req_size)
	    nbytes = -1;
	  else
	    nbytes += xferd;

	  remaining -= req_size;
	  offs++;
	}
    }
  else /* not seek_rw_single_op */
    {
      while (remaining && nbytes >= 0)
	{
	  if (lseek (fd, *offs, SEEK_SET) < 0)
	    nbytes = -1;
	  else
	    {
	      req_size = (file_size - *offs);
	      if (req_size > max_req_size)
		req_size = max_req_size;
	      if (req_size > remaining)
		req_size = remaining;
	      xferd = read (fd, buf, req_size);
	      if (xferd < (ssize_t) req_size)
		nbytes = -1;
	      else
		nbytes += xferd;
	    }

	  remaining -= req_size;
	  offs++;
	}
    }

  return nbytes;
} /* posix_read_oset () */

ssize_t
posix_write_dset (iore_file_t file, const void *buf, const iore_test_t *test)
{
  assert(buf);
  assert(test);

  ssize_t nbytes = 0;

  int fd = file.hdle.fint;
  size_t dset_size = test->wkld.u.dset.my_size;
  size_t req_size;
  ssize_t xferd;

  if (test->wkld.u.dset.type == IORE_WKLD_DSET_CARTESIAN)
    {
      req_size =
	  (test->wkld.u.dset._vars_size
	      * test->wkld.u.dset.u.cart.my_dim_sizes[test->wkld.u.dset.u.cart.num_dims
		  - 1]);
    }
  else /* unsupported dataset type */
    {
      return -1;
    }

  off_t *offs = dset_to_off (&test->wkld.u.dset, test->file_mode);
  if (!offs)
    return -1;
  /* pointer kept for freeing it later */
  off_t *first_off = offs;

  bool seek_rw_single_op = strtob (
      dict_get (&test->afio.params, AFIO_PARAM_SEEK_RW_SINGLE_OP));
  if (seek_rw_single_op)
    {
      while (nbytes < (ssize_t) dset_size && nbytes >= 0)
	{
	  xferd = pwrite (fd, buf + nbytes, req_size, *offs);
	  if (xferd < (ssize_t) req_size)
	    nbytes = -1;
	  else
	    {
	      nbytes += xferd;
	      if (test->write_flush_per_req)
		fsync (fd);
	    }
	  offs++;
	}
    }
  else /* not seek_rw_single_op */
    {
      while (nbytes < (ssize_t) dset_size && nbytes >= 0)
	{
	  if (lseek (fd, *offs, SEEK_SET) < 0)
	    nbytes = -1;
	  else
	    {
	      xferd = write (fd, buf + nbytes, req_size);
	      if (xferd < (ssize_t) req_size)
		nbytes = -1;
	      else
		{
		  nbytes += xferd;
		  if (test->write_flush_per_req)
		    fsync (fd);
		}
	    }
	  offs++;
	}
    }

  if (nbytes > 0 && test->write_flush)
    fsync (fd);

  free (first_off);

  return nbytes;
} /* posix_write_dset () */

ssize_t
posix_read_dset (iore_file_t file, void *buf, const iore_test_t *test)
{
  assert(buf);
  assert(test);

  ssize_t nbytes = 0;

  int fd = file.hdle.fint;
  size_t dset_size = test->wkld.u.dset.my_size;
  size_t req_size;
  ssize_t xferd;

  if (test->wkld.u.dset.type == IORE_WKLD_DSET_CARTESIAN)
    {
      req_size =
	  (test->wkld.u.dset._vars_size
	      * test->wkld.u.dset.u.cart.my_dim_sizes[test->wkld.u.dset.u.cart.num_dims
		  - 1]);
    }
  else /* unsupported dataset type */
    {
      return -1;
    }

  off_t *offs = dset_to_off (&test->wkld.u.dset, test->file_mode);
  if (!offs)
    return -1;
  /* pointer kept for freeing it later */
  off_t *first_off = offs;

  bool seek_rw_single_op = strtob (
      dict_get (&test->afio.params, AFIO_PARAM_SEEK_RW_SINGLE_OP));
  if (seek_rw_single_op)
    {
      while (nbytes < (ssize_t) dset_size && nbytes >= 0)
	{
	  xferd = pread (fd, buf + nbytes, req_size, *offs);
	  if (xferd < (ssize_t) req_size)
	    nbytes = -1;
	  else
	    nbytes += xferd;

	  offs++;
	}
    }
  else /* not seek_rw_single_op */
    {
      while (nbytes < (ssize_t) dset_size && nbytes >= 0)
	{
	  if (lseek (fd, *offs, SEEK_SET) < 0)
	    nbytes = -1;
	  else
	    {
	      xferd = read (fd, buf + nbytes, req_size);
	      if (xferd < (ssize_t) req_size)
		nbytes = -1;
	      else
		nbytes += xferd;
	    }
	  offs++;
	}
    }

  free (first_off);

  return nbytes;
} /* posix_read_dset () */

int
posix_close (iore_file_t *file)
{
  assert(file);

  return close (file->hdle.fint);
} /* posix_close () */

int
posix_remove (iore_file_t file)
{
  return unlink (file.name);
} /* posix_remove () */
