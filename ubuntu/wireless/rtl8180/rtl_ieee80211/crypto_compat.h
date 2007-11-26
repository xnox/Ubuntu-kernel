#ifndef __CRYPTO_COMPAT_H 
#define __CRYPTO_COMPAT_H 1

void crypto_digest_init(struct crypto_tfm *tfm) __deprecated_for_modules;
void crypto_digest_update(struct crypto_tfm *tfm,
			  struct scatterlist *sg, unsigned int nsg);
void crypto_digest_final(struct crypto_tfm *tfm, u8 *out);

static int crypto_digest_setkey(struct crypto_tfm *tfm, const u8 *key,
				unsigned int keylen);
static inline int crypto_digest_setkey(struct crypto_tfm *tfm,
                                       const u8 *key, unsigned int keylen)
{
	return tfm->crt_hash.setkey(crypto_hash_cast(tfm), key, keylen);
}

#endif
