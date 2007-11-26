#include <linux/kernel.h>
#include <linux/scatterlist.h>
#include <linux/crypto.h>
 
void crypto_digest_init(struct crypto_tfm *tfm)
{
	struct crypto_hash *hash = crypto_hash_cast(tfm);
	struct hash_desc desc = { .tfm = hash, .flags = tfm->crt_flags };

	crypto_hash_init(&desc);
}

void crypto_digest_update(struct crypto_tfm *tfm,
			  struct scatterlist *sg, unsigned int nsg)
{
	struct crypto_hash *hash = crypto_hash_cast(tfm);
	struct hash_desc desc = { .tfm = hash, .flags = tfm->crt_flags };
	unsigned int nbytes = 0;
	unsigned int i;

	for (i = 0; i < nsg; i++)
		nbytes += sg[i].length;

	crypto_hash_update(&desc, sg, nbytes);
}

void crypto_digest_final(struct crypto_tfm *tfm, u8 *out)
{
	struct crypto_hash *hash = crypto_hash_cast(tfm);
	struct hash_desc desc = { .tfm = hash, .flags = tfm->crt_flags };

	crypto_hash_final(&desc, out);
}
