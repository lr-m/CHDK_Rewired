#ifndef FILEIO_H
#define FILEIO_H

/* CHDK port: see gbc_port.h */
/* CHDK port: see gbc_port.h */

int read_file(char *filename, uint8_t **buf, size_t *size);
int save_file(char *filename, uint8_t *buf, size_t size);

#endif
