#include <stdlib.h>
#include <zlib.h>

#define TARFILE_NOTAR   0
#define TARFILE_TAR     1
#define TARFILE_TGZ     2

struct tarfile {
  int type;
  FILE * f;
  gzFile * g;
  char * fname;
  char * username;
  char * groupname;
  int uid;
  int gid;
  fpos_t record_start;
  z_off_t gz_record_start;
  int n;
};

int tarfile_open (struct tarfile * tf, const char * fname, int type);
int tarfile_close (struct tarfile * tf);
int tarfile_set_user (struct tarfile * tf, const char * username, int uid, const char * groupname, int gid);
int tarfile_start_txt (struct tarfile * tf, const char * fname);
int tarfile_stop_txt (struct tarfile * tf, const char * fname);
int tarfile_binary (struct tarfile * tf, const char * fname, int numbytes, char * buf);

#define tfprintf(a,...) if (a->type==TARFILE_TGZ) a->n+=gzprintf(a->g,__VA_ARGS__); else a->n+=fprintf(a->f,__VA_ARGS__)
