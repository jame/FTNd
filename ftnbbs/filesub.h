#ifndef _FILESUB_H
#define	_FILESUB_H

/* filesub.h */

FILE		*OpenFareas(int);
int		CheckBytesAvailable(int);
int		iLC(int);
void		Header(void);
void		Sheader(void);
int		ShowOneFile(void);
int		Addfile(char *, int, int);
void		InitTag(void);
void		SetTag(_Tag);
void		Blanker(int);
void		GetstrD(char *, int);
void		Mark(void);
int		UploadB_Home(char *);
char		*GetFileType(char *);
void		Home(void);
int		ScanDirect(char *);
int		ScanArchive(char *, char *);
int		ImportFile(char *, int, int, off_t);
unsigned int	Quota(void);
void		ImportHome(char *);

#endif
