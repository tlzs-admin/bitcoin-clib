#ifndef CHLIB_BASE64_H_
#define CHLIB_BASE64_H_

#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif

ssize_t base64_encode(const void * src, size_t src_len, char ** p_dst);
ssize_t base64_decode(const char * src, size_t src_len, unsigned char ** p_dst);

ssize_t base64_urlencode(const void * src, size_t src_len, char ** p_dst);
ssize_t base64_urldecode(const char * src, size_t src_len, unsigned char ** p_dst);

#ifdef __cplusplus
}
#endif
#endif
