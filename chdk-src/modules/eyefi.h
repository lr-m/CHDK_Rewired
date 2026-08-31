/*
* Based on work by Dave Hansen <dave@sr71.net>
* original licensing:
* This software may be redistributed and/or modified under the terms of
 * the GNU General Public License ("GPL") version 2 as published by the
 * Free Software Foundation.
 */
#ifndef EYEFI_H
#define EYEFI_H

#include <stddef.h>
#include <stdlib.h>

//-------------------------------------------------------------------
// SHA1 / MD5 stuff

typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;

//little endian
#define host_to_be32(n) __builtin_bswap32(n)

#define os_memset memset
#define os_memcpy memcpy
#define os_strlen strlen
#define os_strcpy strcpy

#define SHA1_MAC_LEN 20
#define MD5_MAC_LEN 16
void md5_vector(size_t num_elem, const u8 *addr[], const size_t *len, u8 *mac);
void hmac_md5_vector(const u8 *key, size_t key_len, size_t num_elem, const u8 *addr[], const size_t *len, u8 *mac);
void hmac_md5(const u8 *key, size_t key_len, const u8 *data, size_t data_len, u8 *mac);
void pbkdf2_sha1(const char *passphrase, const char *ssid, size_t ssid_len, int iterations, u8 *buf, size_t buflen);

//-------------------------------------------------------------------
#endif
