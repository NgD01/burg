/* grub-fstest.c - debug tool for filesystem driver */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2008,2009,2010 Free Software Foundation, Inc.
 *
 *  GRUB is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GRUB is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GRUB.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <grub/types.h>
#include <grub/util/misc.h>
#include <grub/misc.h>
#include <grub/device.h>
#include <grub/disk.h>
#include <grub/file.h>
#include <grub/fs.h>
#include <grub/env.h>
#include <grub/term.h>
#include <grub/mm.h>
#include <grub/lib.h>
#include <grub/command.h>
#include <grub/i18n.h>

#include <grub_fstest_init.h>

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include "progname.h"

void
grub_putchar (int c)
{
  putchar (c);
}

int
grub_getkey (void)
{
  return -1;
}

struct grub_handler_class grub_term_input_class;
struct grub_handler_class grub_term_output_class;

void
grub_refresh (void)
{
  fflush (stdout);
}

static grub_err_t
execute_command (char *name, int n, char **args)
{
  grub_command_t cmd;

  cmd = grub_command_find (name);
  if (! cmd)
    grub_util_error ("can\'t find command %s", name);

  return (cmd->func) (cmd, n, args);
}

#define CMD_LS          1
#define CMD_CP          2
#define CMD_CMP         3
#define CMD_HEX         4
#define CMD_CRC         6
#define CMD_BLOCKLIST   7

#define BUF_SIZE  32256

static grub_disk_addr_t skip, leng;

static void
read_file (char *pathname,
	   int (*hook) (grub_off_t ofs, char *buf, int len, void *closure),
	   void *closure)
{
  static char buf[BUF_SIZE];
  grub_file_t file;
  grub_off_t ofs, len;

  if ((pathname[0] == '-') && (pathname[1] == 0))
    {
      grub_device_t dev;

      dev = grub_device_open (0);
      if ((! dev) || (! dev->disk))
        grub_util_error ("can\'t open device");

      grub_util_info ("total sectors : %lld",
                      (unsigned long long) dev->disk->total_sectors);

      if (! leng)
        leng = (dev->disk->total_sectors << GRUB_DISK_SECTOR_BITS) - skip;

      while (leng)
        {
          grub_size_t len;

          len = (leng > BUF_SIZE) ? BUF_SIZE : leng;

          if (grub_disk_read (dev->disk, 0, skip, len, buf))
            grub_util_error ("disk read fails at offset %lld, length %d",
                             skip, len);

          if (hook (skip, buf, len, closure))
            break;

          skip += len;
          leng -= len;
        }

      grub_device_close (dev);
      return;
    }

  file = grub_file_open (pathname);
  if (!file)
    {
      grub_util_error ("cannot open file %s", pathname);
      return;
    }

  grub_util_info ("file size : %lld", (unsigned long long) file->size);

  if (skip > file->size)
    {
      grub_util_error ("invalid skip value %lld", (unsigned long long) skip);
      return;
    }

  ofs = skip;
  len = file->size - skip;
  if ((leng) && (leng < len))
    len = leng;

  file->offset = skip;

  while (len)
    {
      grub_ssize_t sz;

      sz = grub_file_read (file, buf, (len > BUF_SIZE) ? BUF_SIZE : len);
      if (sz < 0)
	{
	  grub_util_error ("read error at offset %llu", ofs);
	  break;
	}

      if ((sz == 0) || (hook (ofs, buf, sz, closure)))
	break;

      ofs += sz;
      len -= sz;
    }

  grub_file_close (file);
}

static int
cp_hook (grub_off_t ofs __attribute__ ((unused)), char *buf, int len,
	 void *closure)
{
  FILE *file = closure;

  if ((int) fwrite (buf, 1, len, file) != len)
    {
      grub_util_error ("write error");
      return 1;
    }

  return 0;
}

static void
cmd_cp (char *src, char *dest)
{
  FILE *ff;

  ff = fopen (dest, "wb");
  if (ff == NULL)
    {
      grub_util_error ("open error");
      return;
    }
  read_file (src, cp_hook, ff);
  fclose (ff);
}

static int
cmp_hook (grub_off_t ofs, char *buf, int len, void *closure)
{
  FILE *file = closure;
  static char buf_1[BUF_SIZE];

  if ((int) fread (buf_1, 1, len, file) != len)
    {
      grub_util_error ("read error at offset %llu", ofs);
      return 1;
    }

  if (grub_memcmp (buf, buf_1, len))
    {
      int i;

      for (i = 0; i < len; i++, ofs++)
	if (buf_1[i] != buf[i])
	  {
	    grub_util_error ("compare fail at offset %llu", ofs);
	    return 1;
	  }
    }
  return 0;
}

static void
cmd_cmp (char *src, char *dest)
{
  FILE *ff;

  ff = fopen (dest, "rb");
  if (ff == NULL)
    {
      grub_util_error ("open error");
      return;
    }

  if ((skip) && (fseeko (ff, skip, SEEK_SET)))
    grub_util_error ("seek error");

  read_file (src, cmp_hook, ff);
  fclose (ff);
}

int hex_hook (grub_off_t ofs, char *buf, int len,
	      void *closure __attribute__ ((unused)))
{
  hexdump (ofs, buf, len);
  return 0;
}

static void
cmd_hex (char *pathname)
{
  read_file (pathname, hex_hook, 0);
}

static int
crc_hook (grub_off_t ofs __attribute__ ((unused)), char *buf, int len,
	  void *closure)
{
  grub_uint32_t *crc = closure;

  *crc = grub_getcrc32 (*crc, buf, len);
  return 0;
}

static void
cmd_crc (char *pathname)
{
  grub_uint32_t crc = 0;

  read_file (pathname, crc_hook, &crc);
  printf ("%08x\n", crc);
}

static void
fstest (char **images, int num_disks, int cmd, int n, char **args)
{
  char *host_file;
  char *loop_name;
  char *argv[3];
  int i;

  argv[0] = "-p";

  for (i = 0; i < num_disks; i++)
    {
      loop_name = grub_xasprintf ("loop%d", i);
      if (!loop_name)
	grub_util_error (grub_errmsg);

      host_file = grub_xasprintf ("(host)%s", images[i]);
      if (!host_file)
	grub_util_error (grub_errmsg);

      argv[1] = loop_name;
      argv[2] = host_file;

      if (execute_command ("loopback", 3, argv))
        grub_util_error ("loopback command fails");

      grub_free (loop_name);
      grub_free (host_file);
    }

  grub_lvm_fini ();
  grub_mdraid_fini ();
  grub_raid_fini ();
  grub_raid_init ();
  grub_mdraid_init ();
  grub_lvm_init ();

  switch (cmd)
    {
    case CMD_LS:
      execute_command ("ls", n, args);
      break;
    case CMD_CP:
      cmd_cp (args[0], args[1]);
      break;
    case CMD_CMP:
      cmd_cmp (args[0], args[1]);
      break;
    case CMD_HEX:
      cmd_hex (args[0]);
      break;
    case CMD_CRC:
      cmd_crc (args[0]);
      break;
    case CMD_BLOCKLIST:
      execute_command ("blocklist", n, args);
      grub_printf ("\n");
    }

  argv[0] = "-d";

  for (i = 0; i < num_disks; i++)
    {
      loop_name = grub_xasprintf ("loop%d", i);
      if (!loop_name)
	grub_util_error (grub_errmsg);

      argv[1] = loop_name;

      execute_command ("loopback", 2, argv);

      grub_free (loop_name);
    }
}

static struct option options[] = {
  {"root", required_argument, 0, 'r'},
  {"skip", required_argument, 0, 's'},
  {"length", required_argument, 0, 'n'},
  {"diskcount", required_argument, 0, 'c'},
  {"debug", required_argument, 0, 'd'},
  {"help", no_argument, 0, 'h'},
  {"version", no_argument, 0, 'V'},
  {"verbose", no_argument, 0, 'v'},
  {0, 0, 0, 0}
};

static void
usage (int status)
{
  if (status)
    fprintf (stderr, "Try `%s --help' for more information.\n", program_name);
  else
    printf ("\
Usage: %s [OPTION]... IMAGE_PATH COMMANDS\n\
\n\
Debug tool for filesystem driver.\n\
\nCommands:\n\
  ls PATH                   list files in PATH\n\
  cp FILE LOCAL             copy FILE to local file LOCAL\n\
  cmp FILE LOCAL            compare FILE with local file LOCAL\n\
  hex FILE                  Hex dump FILE\n\
  crc FILE                  Get crc32 checksum of FILE\n\
  blocklist FILE            display blocklist of FILE\n\
\nOptions:\n\
  -r, --root=DEVICE_NAME    set root device\n\
  -s, --skip=N              skip N bytes from output file\n\
  -n, --length=N            handle N bytes in output file\n\
  -c, --diskcount=N         N input files\n\
  -d, --debug=S             Set debug environment variable\n\
  -h, --help                display this message and exit\n\
  -V, --version             print version information and exit\n\
  -v, --verbose             print verbose messages\n\
\n\
Report bugs to <%s>.\n", program_name, PACKAGE_BUGREPORT);

  exit (status);
}

int
main (int argc, char *argv[])
{
  char *debug_str = NULL, *root = NULL, *default_root, *alloc_root;
  int i, cmd, num_opts, image_index, num_disks = 1;

  set_program_name (argv[0]);

  grub_util_init_nls ();

  /* Find the first non option entry.  */
  for (num_opts = 1; num_opts < argc; num_opts++)
    if (argv[num_opts][0] == '-')
      {
        if ((argv[num_opts][2] == 0) && (num_opts < argc - 1) &&
            ((argv[num_opts][1] == 'r') ||
             (argv[num_opts][1] == 's') ||
             (argv[num_opts][1] == 'n') ||
             (argv[num_opts][1] == 'c') ||
             (argv[num_opts][1] == 'd')))
            num_opts++;
      }
    else
      break;

  /* Check for options.  */
  while (1)
    {
      int c = getopt_long (num_opts, argv, "r:s:n:c:d:hVv", options, 0);
      char *p;

      if (c == -1)
	break;
      else
	switch (c)
	  {
	  case 'r':
	    root = optarg;
	    break;

	  case 's':
	    skip = grub_strtoul (optarg, &p, 0);
            if (*p == 's')
              skip <<= GRUB_DISK_SECTOR_BITS;
	    break;

	  case 'n':
	    leng = grub_strtoul (optarg, &p, 0);
            if (*p == 's')
              leng <<= GRUB_DISK_SECTOR_BITS;
	    break;

          case 'c':
            num_disks = grub_strtoul (optarg, NULL, 0);
            if (num_disks < 1)
              {
                fprintf (stderr, "Invalid disk count.\n");
                usage (1);
              }
            break;

          case 'd':
            debug_str = optarg;
            break;

	  case 'h':
	    usage (0);
	    break;

	  case 'V':
	    printf ("%s (%s) %s\n", program_name, PACKAGE_NAME, PACKAGE_VERSION);
	    return 0;

	  case 'v':
	    verbosity++;
	    break;

	  default:
	    usage (1);
	    break;
	  }
    }

  /* Obtain PATH.  */
  if (optind + num_disks - 1 >= argc)
    {
      fprintf (stderr, "Not enough pathname.\n");
      usage (1);
    }

  image_index = optind;
  for (i = 0; i < num_disks; i++, optind++)
    if (argv[optind][0] != '/')
      {
        fprintf (stderr, "Must use absolute path.\n");
        usage (1);
      }

  cmd = 0;
  if (optind < argc)
    {
      int nparm = 0;

      if (!grub_strcmp (argv[optind], "ls"))
        {
          cmd = CMD_LS;
        }
      else if (!grub_strcmp (argv[optind], "cp"))
	{
	  cmd = CMD_CP;
          nparm = 2;
	}
      else if (!grub_strcmp (argv[optind], "cmp"))
	{
	  cmd = CMD_CMP;
          nparm = 2;
	}
      else if (!grub_strcmp (argv[optind], "hex"))
	{
	  cmd = CMD_HEX;
          nparm = 1;
	}
      else if (!grub_strcmp (argv[optind], "crc"))
	{
	  cmd = CMD_CRC;
          nparm = 1;
	}
      else if (!grub_strcmp (argv[optind], "blocklist"))
	{
	  cmd = CMD_BLOCKLIST;
          nparm = 1;
	}
      else
	{
	  fprintf (stderr, "Invalid command %s.\n", argv[optind]);
	  usage (1);
	}

      if (optind + 1 + nparm > argc)
	{
	  fprintf (stderr, "Invalid parameter for command %s.\n",
		   argv[optind]);
	  usage (1);
	}

      optind++;
    }
  else
    {
      fprintf (stderr, "No command is specified.\n");
      usage (1);
    }

  /* Initialize all modules. */
  grub_init_all ();

  if (debug_str)
    grub_env_set ("debug", debug_str);

  default_root = (num_disks == 1) ? "loop0" : "md0";
  alloc_root = 0;
  if (root)
    {
      if ((*root >= '0') && (*root <= '9'))
        {
          alloc_root = xmalloc (strlen (default_root) + strlen (root) + 2);

          sprintf (alloc_root, "%s,%s", default_root, root);
          root = alloc_root;
        }
    }
  else
    root = default_root;

  grub_env_set ("root", root);

  if (alloc_root)
    free (alloc_root);

  /* Do it.  */
  fstest (argv + image_index, num_disks, cmd, argc - optind, argv + optind);

  /* Free resources.  */
  grub_fini_all ();

  return 0;
}
