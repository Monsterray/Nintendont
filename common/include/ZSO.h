/*
ZSO.h for Nintendont (Kernel and Loader)

ZSO ("ZISO") compressed disc images: a block index followed by
LZ4-compressed blocks. Based on FIX94/Nintendont#1170 by
JoseAaronLopezGarcia.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation version 2.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/
#ifndef __ZSO_H__
#define __ZSO_H__

/*
 * Header (all fields little-endian):
 * 0x00  "ZISO"
 * 0x04  header size (0x18)
 * 0x08  uncompressed size (u64)
 * 0x10  block size
 * 0x14  version, index alignment shift, 2 reserved bytes
 * 0x18  index: one u32 per block plus one end entry.
 *       Bit 31 set = block stored uncompressed.
 *       File offset = (entry & 0x7FFFFFFF) << shift.
 */
#define ZSO_MAGIC		0x5A49534F	/* "ZISO" */
#define ZSO_HEADER_SIZE		0x18
#define ZSO_PLAIN		0x80000000
#define ZSO_BLOCK_MIN		0x800
#define ZSO_BLOCK_MAX		0x8000

static inline unsigned int ZSO_le32(const unsigned char *p)
{
	return p[0] | (p[1] << 8) | (p[2] << 16) | ((unsigned int)p[3] << 24);
}

/* Block size from a header, or 0 if this is not a usable ZSO image. */
static inline unsigned int ZSO_BlockSize(const unsigned char *hdr)
{
	unsigned int bs = ZSO_le32(hdr + 0x10);
	if (hdr[0] != 'Z' || hdr[1] != 'I' || hdr[2] != 'S' || hdr[3] != 'O' ||
	    ZSO_le32(hdr + 4) != ZSO_HEADER_SIZE ||
	    bs < ZSO_BLOCK_MIN || bs > ZSO_BLOCK_MAX || (bs & (bs - 1)))
		return 0;
	return bs;
}

/*
 * Decode one LZ4 block into dst, stopping once dst is full, so the
 * alignment padding after a block is ignored. Every length and offset
 * is checked against both buffers.
 * Returns the number of bytes written, or -1 if the data is corrupt.
 */
static inline int ZSO_LZ4Decode(const unsigned char *src, unsigned int srcLen,
				unsigned char *dst, unsigned int dstLen)
{
	const unsigned char *ip = src, *iend = src + srcLen;
	unsigned char *op = dst, *oend = dst + dstLen;
	while (ip < iend && op < oend)
	{
		unsigned int token = *ip++, len = token >> 4, b;
		if (len == 15)
			do { if (ip >= iend) return -1; b = *ip++; len += b; } while (b == 255);
		if (len > (unsigned int)(iend - ip) || len > (unsigned int)(oend - op))
			return -1;
		while (len--) *op++ = *ip++;
		if (ip >= iend || op >= oend)
			break;	// the last sequence is literals only

		if (iend - ip < 2) return -1;
		unsigned int off = ip[0] | (ip[1] << 8);
		ip += 2;
		if (off == 0 || off > (unsigned int)(op - dst)) return -1;
		len = token & 15;
		if (len == 15)
			do { if (ip >= iend) return -1; b = *ip++; len += b; } while (b == 255);
		len += 4;
		if (len > (unsigned int)(oend - op)) return -1;
		const unsigned char *m = op - off;
		while (len--) *op++ = *m++;
	}
	return op - dst;
}

#endif
