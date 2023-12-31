/* $OpenBSD: ssh-dss.c,v 1.31 2014/02/02 03:44:31 djm Exp $ */
/*
 * Copyright (c) 2000 Markus Friedl.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "includes.h"

#include <sys/types.h>

#ifdef USING_WOLFSSL
#include <wolfssl/openssl/bn.h>
#include <wolfssl/openssl/evp.h>
#else
#include <openssl/bn.h>
#include <openssl/evp.h>
#endif

#include <stdarg.h>
#include <string.h>

#include "xmalloc.h"
#include "buffer.h"
#include "compat.h"
#include "log.h"
#include "key.h"
#include "digest.h"

#define INTBLOB_LEN	20
#define SIGBLOB_LEN	(2*INTBLOB_LEN)

int
ssh_dss_sign(const Key *key, u_char **sigp, u_int *lenp,
    const u_char *data, u_int datalen)
{
#ifdef USING_WOLFSSL
	int ret;
#else
	DSA_SIG *sig;
	u_int rlen, slen, dlen = ssh_digest_bytes(SSH_DIGEST_SHA1);
#endif /* USING_WOLFSSL */
	u_char digest[SSH_DIGEST_MAX_LENGTH], sigblob[SIGBLOB_LEN];
	u_int len;
	Buffer b;

	if (key == NULL || key_type_plain(key->type) != KEY_DSA ||
	    key->dsa == NULL) {
		error("%s: no DSA key", __func__);
		return -1;
	}

	if (ssh_digest_memory(SSH_DIGEST_SHA1, data, datalen,
	    digest, sizeof(digest)) != 0) {
		error("%s: ssh_digest_memory failed", __func__);
		return -1;
	}

#ifdef USING_WOLFSSL
	memset(sigblob, 0, SIGBLOB_LEN);
	ret = wolfSSL_DSA_do_sign(digest, sigblob, key->dsa);
	explicit_bzero(digest, sizeof(digest));

	if (ret != 1) {
		error("wolfSSL_DSA_do_sign: sign failed");
		return -1;
	}
#else /* USING_WOLFSSL */
	sig = DSA_do_sign(digest, dlen, key->dsa);
	explicit_bzero(digest, sizeof(digest));

	if (sig == NULL) {
		error("ssh_dss_sign: sign failed");
		return -1;
	}
#endif /* USING_WOLFSSL */

#ifndef USING_WOLFSSL
	rlen = BN_num_bytes(sig->r);
	slen = BN_num_bytes(sig->s);
	if (rlen > INTBLOB_LEN || slen > INTBLOB_LEN) {
		error("bad sig size %u %u", rlen, slen);
		DSA_SIG_free(sig);
		return -1;
	}
	explicit_bzero(sigblob, SIGBLOB_LEN);
	BN_bn2bin(sig->r, sigblob+ SIGBLOB_LEN - INTBLOB_LEN - rlen);
	BN_bn2bin(sig->s, sigblob+ SIGBLOB_LEN - slen);
	DSA_SIG_free(sig);
#endif /* USING_WOLFSSL */

	if (datafellows & SSH_BUG_SIGBLOB) {
		if (lenp != NULL)
			*lenp = SIGBLOB_LEN;
		if (sigp != NULL) {
			*sigp = xmalloc(SIGBLOB_LEN);
			memcpy(*sigp, sigblob, SIGBLOB_LEN);
		}
	} else {
		/* ietf-drafts */
		buffer_init(&b);
		buffer_put_cstring(&b, "ssh-dss");
		buffer_put_string(&b, sigblob, SIGBLOB_LEN);
		len = buffer_len(&b);
		if (lenp != NULL)
			*lenp = len;
		if (sigp != NULL) {
			*sigp = xmalloc(len);
			memcpy(*sigp, buffer_ptr(&b), len);
		}
		buffer_free(&b);
	}
	return 0;
}
int
ssh_dss_verify(const Key *key, const u_char *signature, u_int signaturelen,
    const u_char *data, u_int datalen)
{
#ifdef USING_WOLFSSL
	int dsacheck = 0;
#else
	DSA_SIG *sig;
	u_int dlen = ssh_digest_bytes(SSH_DIGEST_SHA1);
#endif /* USING_WOLFSSL */
	u_char digest[SSH_DIGEST_MAX_LENGTH], *sigblob;
	u_int len;
	int rlen, ret;
	Buffer b;

	if (key == NULL || key_type_plain(key->type) != KEY_DSA ||
	    key->dsa == NULL) {
		error("%s: no DSA key", __func__);
		return -1;
	}

	/* fetch signature */
	if (datafellows & SSH_BUG_SIGBLOB) {
		sigblob = xmalloc(signaturelen);
		memcpy(sigblob, signature, signaturelen);
		len = signaturelen;
	} else {
		/* ietf-drafts */
		char *ktype;
		buffer_init(&b);
		buffer_append(&b, signature, signaturelen);
		ktype = buffer_get_cstring(&b, NULL);
		if (strcmp("ssh-dss", ktype) != 0) {
			error("%s: cannot handle type %s", __func__, ktype);
			buffer_free(&b);
			free(ktype);
			return -1;
		}
		free(ktype);
		sigblob = buffer_get_string(&b, &len);
		rlen = buffer_len(&b);
		buffer_free(&b);
		if (rlen != 0) {
			error("%s: remaining bytes in signature %d",
			    __func__, rlen);
			free(sigblob);
			return -1;
		}
	}

	if (len != SIGBLOB_LEN) {
		fatal("bad sigbloblen %u != SIGBLOB_LEN", len);
	}

#ifndef USING_WOLFSSL
	/* parse signature */
	if ((sig = DSA_SIG_new()) == NULL)
		fatal("%s: DSA_SIG_new failed", __func__);
	if ((sig->r = BN_new()) == NULL)
		fatal("%s: BN_new failed", __func__);
	if ((sig->s = BN_new()) == NULL)
		fatal("ssh_dss_verify: BN_new failed");
	if ((BN_bin2bn(sigblob, INTBLOB_LEN, sig->r) == NULL) ||
	    (BN_bin2bn(sigblob+ INTBLOB_LEN, INTBLOB_LEN, sig->s) == NULL))
		fatal("%s: BN_bin2bn failed", __func__);

	/* clean up */
	explicit_bzero(sigblob, len);
	free(sigblob);
#endif /* USING_WOLFSSL */

	/* sha1 the data */
	if (ssh_digest_memory(SSH_DIGEST_SHA1, data, datalen,
	    digest, sizeof(digest)) != 0) {
		error("%s: digest_memory failed", __func__);
		return -1;
	}

#ifdef USING_WOLFSSL
	ret = wolfSSL_DSA_do_verify(digest, sigblob, key->dsa, &dsacheck);
	explicit_bzero(digest, sizeof(digest));

	debug("%s: signature %s", __func__,
	    (ret == 1 && dsacheck == 1) ? "correct" : ret == 1 ? "incorrect" : "error");
#else
	ret = DSA_do_verify(digest, dlen, sig, key->dsa);
	explicit_bzero(digest, sizeof(digest));

	DSA_SIG_free(sig);

	debug("%s: signature %s", __func__,
	    ret == 1 ? "correct" : ret == 0 ? "incorrect" : "error");
#endif /* USING_WOLFSSL */
	return ret;
}
