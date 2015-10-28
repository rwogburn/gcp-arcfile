/*
 * RWO 140625 - write tarfiles, including text files.
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "tarfile.h"

char record_header[512];

int tarfile_open (struct tarfile * tf, const char * fname, int type)
{
  tf->f = NULL;
  tf->g = NULL;
  tf->type = type;
  tf->username = NULL;
  tf->groupname = NULL;
  tf->uid = 0;
  tf->gid = 0;
  tf->fname = NULL;

  if (type == TARFILE_TAR)
  {
    tf->f = fopen (fname, "wb");
  }
  else if (type == TARFILE_TGZ)
  {
    tf->g = gzopen (fname, "wb");
  }
  else if (type == TARFILE_NOTAR)
  {
    tf->fname = malloc (strlen(fname)+1);
    strncpy(tf->fname, fname, strlen(fname));
  }
  else
  {
    printf ("Unrecognized tarfile type %d.\n", type);
    return -1;
  }

  return 0;
}

int tarfile_close (struct tarfile * tf)
{
  for (int i=0; i<512; i++)
    record_header[i] = 0;

  if (tf->type == TARFILE_TAR)
  {
    fwrite (record_header, sizeof(char), 512, tf->f);
    fwrite (record_header, sizeof(char), 512, tf->f);
    fclose (tf->f);
    tf->f = NULL;
  }
  else if (tf->type == TARFILE_TGZ)
  {
    gzwrite (tf->g, record_header, 512);
    gzwrite (tf->g, record_header, 512);
    gzclose (tf->g);
    tf->g = NULL;
  }
  else if (tf->type == TARFILE_NOTAR)
  {
  }
  else
  {
    printf ("Unrecognized tarfile type %d.\n", tf->type);
    return -1;
  }

  return 0;
}

int tarfile_set_user (struct tarfile * tf, const char * username, int uid, const char * groupname, int gid)
{
  tf->uid = uid;
  tf->gid = gid;
  if (username != NULL)
    tf->username = username;
  if (groupname != NULL)
    tf->groupname = groupname;

  return 0;
}

int fill_in_header (struct tarfile * tf, const char * fname)
{
  int i, n;
  unsigned int chk;

  /* Zero out entire header record */
  for (i=0; i<512; i++)
    record_header[i] = 0;

  /* File name: may need to use prefix */
  n=strlen(fname);
  if (n<=100)
    strncpy(record_header+0,fname,100);
  else
  {
    strncpy(record_header+0,fname+n-100,100);
    strncpy(record_header+345,fname,n-100);
  }

  /* File mode: -rw-rw-r-- */
  sprintf(record_header+100,"%06o ",0644);

  /* User and group ID */
  sprintf(record_header+108,"%06o ",tf->uid);
  sprintf(record_header+116,"%06o ",tf->gid);

  /* File size */
  sprintf(record_header+124,"%011o ",tf->n);

  /* Modification time */
  sprintf(record_header+136,"%011o ",time(NULL));

  /* Record header checksum: tar file arcana sez set to all spaces */
  sprintf(record_header+148,"      ");

  /* File type */
  sprintf(record_header+156,"0");

  /* USTAR indicator w/ version */
  sprintf(record_header+257,"ustar%c00",0,0);

  /* User and group names */
  if (tf->username != NULL)
    sprintf(record_header+265,"%s",tf->username);
  if (tf->groupname != NULL)
    sprintf(record_header+297,"%s",tf->groupname);

  /* Device numbers -- all zero */
  sprintf(record_header+329,"%.6o ",0);
  sprintf(record_header+337,"%.6o ",0);

  /* Calculate checksum and fill in again */
  chk=0;
  for (i=0; i<512; i++)
    chk=chk+record_header[i];
  /* No good reason why */
  chk += 64;
  chk %= 01000000;
  sprintf(record_header+148,"%06o%c 0",chk);

  return 0;
}

int tarfile_start_txt (struct tarfile * tf, const char * fname)
{
  tf->n = 0;
  if (tf->type == TARFILE_TAR)
  {
    fill_in_header (tf, fname);
    fgetpos (tf->f, &(tf->record_start));
    fwrite (record_header,sizeof(char),512,tf->f);
  }
  else if (tf->type == TARFILE_TGZ)
  {
    fill_in_header (tf, fname);
    tf->gz_record_start = gztell (tf->g);
    gzwrite (tf->g,record_header,512);
  } 
  else if (tf->type == TARFILE_NOTAR)
  {
    char tmp[256];
    snprintf(tmp,255,"%s/%s",tf->fname,fname);
    tf->f=fopen(tmp,"wt");
  }
  else
  {
    printf ("Unrecognized tarfile type %d.\n", tf->type);
    return -1;
  }

  return 0;
}

int tarfile_stop_txt (struct tarfile * tf, const char * fname)
{
  if (tf->type == TARFILE_TAR)
  {
    fpos_t record_end;
    int nzero = 512 - ((tf->n + 511) % 512) - 1;
    while (nzero > 0)
    {
      fputc (0, tf->f);
      nzero--;
    }
    fgetpos (tf->f, &(record_end));
    fill_in_header (tf, fname);
    fsetpos (tf->f, &(tf->record_start));
    fwrite (record_header,sizeof(char),512,tf->f);
    fsetpos (tf->f, &(record_end));
  }
  else if (tf->type == TARFILE_TGZ)
  {
    z_off_t gz_record_end;
    int nzero = 512 - ((tf->n + 1) % 512) + 1;
    while (nzero > 0)
    {
      gzputc (tf->g, 0);
      nzero--;
    }
    gz_record_end = gztell (tf->g);
    fill_in_header (tf, fname);
    gzseek (tf->g, tf->gz_record_start, SEEK_SET);
    gzwrite (tf->g,record_header,512);
    gzseek (tf->g, gz_record_end, SEEK_SET);
  }
  else if (tf->type == TARFILE_NOTAR)
  {
    fclose (tf->f);
    tf->f = NULL;
  }
  else
  {
    printf ("Unrecognized tarfile type %d.\n", tf->type);
    return -1;
  }
  tf->n = 0;

  return 0;
}

int tarfile_binary (struct tarfile * tf, const char * fname, int numbytes, char * buf)
{
  int nzero = 512 - ((numbytes + 511) % 512) - 1;

  tf->n = numbytes;
  fill_in_header (tf, fname);
  if (tf->type == TARFILE_TAR)
  {
    fwrite (record_header,sizeof(char),512,tf->f);
    fwrite (buf,sizeof(char),numbytes,tf->f);
    while (nzero > 0)
    {
      fputc (0, tf->f);
      nzero--;
    }
  }
  else if (tf->type == TARFILE_TGZ)
  {
    gzwrite (tf->g,record_header,512);
    gzwrite (tf->g,buf,numbytes);
    while (nzero > 0)
    {
      gzputc (tf->g, 0);
      nzero--;
    }
  }
  else if (tf->type == TARFILE_NOTAR)
  {
    char tmp[256];
    snprintf(tmp,255,"%s/%s",tf->fname,fname);
    tf->f=fopen(tmp,"wb");
    fwrite (buf,sizeof(char),numbytes,tf->f);
    fclose(tf->f);
    tf->f=NULL;
  }
  else
  {
    printf ("Unrecognized tarfile type %d.\n", tf->type);
    return -1;
  }

  return 0;
}

