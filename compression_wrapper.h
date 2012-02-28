#ifndef __C_CREATEREPOLIB_COMPRESSION_WRAPPER_H__
#define __C_CREATEREPOLIB_COMPRESSION_WRAPPER_H__

#include <stdio.h>
#include <zlib.h>
#include <bzlib.h>

typedef enum {
    AUTO_DETECT_COMPRESSION,
    UNKNOWN_COMPRESSION,
    NO_COMPRESSION,
    GZ_COMPRESSION,
    BZ2_COMPRESSION,
} CompressionType;

#define CW_MODE_READ    0
#define CW_MODE_WRITE   1

typedef struct {
    CompressionType type;
    void *FILE;  // Pointer to gzFile, BZFILE or plain FILE
    int mode;
} CW_FILE;


#define CW_OK   0
#define CW_ERR -1

CompressionType detect_compression(const char* filename);

CW_FILE *cw_open(const char *filename, int mode, CompressionType comtype);

int cw_read(CW_FILE *cw_file, void *buffer, unsigned int len);
int cw_write(CW_FILE *cw_file, void *buffer, unsigned int len);
int cw_puts(CW_FILE *cw_file, const char *str);
int cw_printf(CW_FILE *cw_file, const char *format, ...);

int cw_close(CW_FILE *cw_file);


#endif /* __C_CREATEREPOLIB_COMPRESSION_WRAPPER_H__ */
