LIBSODIUM_SRC_DIR = $(call select_from_ports,libsodium)/src/lib/libsodium/src/libsodium

LIBS    += base jitterentropy
INC_DIR += $(LIBSODIUM_SRC_DIR)/include/sodium \
           $(LIBSODIUM_SRC_DIR)/../../builds/msvc \
           $(REP_DIR)/src/lib/libsodium/include
CC_OPT  += -Wno-unused-function -Wno-unused-variable
SRC_C    = crypto_aead/aes256gcm/aesni/aead_aes256gcm_aesni.c \
           crypto_aead/chacha20poly1305/sodium/aead_chacha20poly1305.c \
           crypto_auth/crypto_auth.c \
           crypto_auth/hmacsha256/auth_hmacsha256_api.c \
           crypto_auth/hmacsha256/cp/hmac_hmacsha256.c \
           crypto_auth/hmacsha256/cp/verify_hmacsha256.c \
           crypto_auth/hmacsha512256/auth_hmacsha512256_api.c \
           crypto_auth/hmacsha512256/cp/hmac_hmacsha512256.c \
           crypto_auth/hmacsha512256/cp/verify_hmacsha512256.c \
           crypto_auth/hmacsha512/auth_hmacsha512_api.c \
           crypto_auth/hmacsha512/cp/hmac_hmacsha512.c \
           crypto_auth/hmacsha512/cp/verify_hmacsha512.c \
           crypto_box/crypto_box.c \
           crypto_box/crypto_box_easy.c \
           crypto_box/crypto_box_seal.c \
           crypto_box/curve25519xsalsa20poly1305/box_curve25519xsalsa20poly1305_api.c \
           crypto_box/curve25519xsalsa20poly1305/ref/after_curve25519xsalsa20poly1305.c \
           crypto_box/curve25519xsalsa20poly1305/ref/before_curve25519xsalsa20poly1305.c \
           crypto_box/curve25519xsalsa20poly1305/ref/box_curve25519xsalsa20poly1305.c \
           crypto_box/curve25519xsalsa20poly1305/ref/keypair_curve25519xsalsa20poly1305.c \
           crypto_core/curve25519/ref10/curve25519_ref10.c \
           crypto_core/hsalsa20/core_hsalsa20_api.c \
           crypto_core/hsalsa20/ref2/core_hsalsa20.c \
           crypto_core/salsa2012/core_salsa2012_api.c \
           crypto_core/salsa2012/ref/core_salsa2012.c \
           crypto_core/salsa208/core_salsa208_api.c \
           crypto_core/salsa208/ref/core_salsa208.c \
           crypto_core/salsa20/core_salsa20_api.c \
           crypto_core/salsa20/ref/core_salsa20.c \
           crypto_generichash/blake2/generichash_blake2_api.c \
           crypto_generichash/blake2/ref/blake2b-compress-ref.c \
           crypto_generichash/blake2/ref/blake2b-compress-sse41.c \
           crypto_generichash/blake2/ref/blake2b-compress-ssse3.c \
           crypto_generichash/blake2/ref/blake2b-ref.c \
           crypto_generichash/blake2/ref/generichash_blake2b.c \
           crypto_generichash/crypto_generichash.c \
           crypto_hash/crypto_hash.c \
           crypto_hash/sha256/cp/hash_sha256.c \
           crypto_hash/sha256/hash_sha256_api.c \
           crypto_hash/sha512/cp/hash_sha512.c \
           crypto_hash/sha512/hash_sha512_api.c \
           crypto_onetimeauth/crypto_onetimeauth.c \
           crypto_onetimeauth/poly1305/donna/poly1305_donna.c \
           crypto_onetimeauth/poly1305/onetimeauth_poly1305.c \
           crypto_onetimeauth/poly1305/sse2/poly1305_sse2.c \
           crypto_pwhash/scryptsalsa208sha256/crypto_scrypt-common.c \
           crypto_pwhash/scryptsalsa208sha256/nosse/pwhash_scryptsalsa208sha256_nosse.c \
           crypto_pwhash/scryptsalsa208sha256/pbkdf2-sha256.c \
           crypto_pwhash/scryptsalsa208sha256/pwhash_scryptsalsa208sha256.c \
           crypto_pwhash/scryptsalsa208sha256/scrypt_platform.c \
           crypto_pwhash/scryptsalsa208sha256/sse/pwhash_scryptsalsa208sha256_sse.c \
           crypto_scalarmult/crypto_scalarmult.c \
           crypto_scalarmult/curve25519/donna_c64/curve25519_donna_c64.c \
           crypto_scalarmult/curve25519/ref10/x25519_ref10.c \
           crypto_scalarmult/curve25519/sandy2x/curve25519_sandy2x.c \
           crypto_scalarmult/curve25519/sandy2x/fe51_invert.c \
           crypto_scalarmult/curve25519/sandy2x/fe_frombytes_sandy2x.c \
           crypto_scalarmult/curve25519/scalarmult_curve25519.c \
           crypto_secretbox/crypto_secretbox.c \
           crypto_secretbox/crypto_secretbox_easy.c \
           crypto_secretbox/xsalsa20poly1305/ref/box_xsalsa20poly1305.c \
           crypto_secretbox/xsalsa20poly1305/secretbox_xsalsa20poly1305_api.c \
           crypto_shorthash/crypto_shorthash.c \
           crypto_shorthash/siphash24/ref/shorthash_siphash24.c \
           crypto_shorthash/siphash24/shorthash_siphash24_api.c \
           crypto_sign/crypto_sign.c \
           crypto_sign/ed25519/ref10/keypair.c \
           crypto_sign/ed25519/ref10/obsolete.c \
           crypto_sign/ed25519/ref10/open.c \
           crypto_sign/ed25519/ref10/sign.c \
           crypto_sign/ed25519/sign_ed25519_api.c \
           crypto_stream/aes128ctr/portable/afternm_aes128ctr.c \
           crypto_stream/aes128ctr/portable/beforenm_aes128ctr.c \
           crypto_stream/aes128ctr/portable/common_aes128ctr.c \
           crypto_stream/aes128ctr/portable/consts_aes128ctr.c \
           crypto_stream/aes128ctr/portable/int128_aes128ctr.c \
           crypto_stream/aes128ctr/portable/stream_aes128ctr.c \
           crypto_stream/aes128ctr/portable/xor_afternm_aes128ctr.c \
           crypto_stream/aes128ctr/stream_aes128ctr_api.c \
           crypto_stream/chacha20/ref/stream_chacha20_ref.c \
           crypto_stream/chacha20/stream_chacha20.c \
           crypto_stream/chacha20/vec/stream_chacha20_vec.c \
           crypto_stream/crypto_stream.c \
           crypto_stream/salsa2012/ref/stream_salsa2012.c \
           crypto_stream/salsa2012/ref/xor_salsa2012.c \
           crypto_stream/salsa2012/stream_salsa2012_api.c \
           crypto_stream/salsa208/ref/stream_salsa208.c \
           crypto_stream/salsa208/ref/xor_salsa208.c \
           crypto_stream/salsa208/stream_salsa208_api.c \
           crypto_stream/salsa20/ref/stream_salsa20_ref.c \
           crypto_stream/salsa20/ref/xor_salsa20_ref.c \
           crypto_stream/salsa20/stream_salsa20_api.c \
           crypto_stream/xsalsa20/ref/stream_xsalsa20.c \
           crypto_stream/xsalsa20/ref/xor_xsalsa20.c \
           crypto_stream/xsalsa20/stream_xsalsa20_api.c \
           crypto_verify/16/ref/verify_16.c \
           crypto_verify/16/verify_16_api.c \
           crypto_verify/32/ref/verify_32.c \
           crypto_verify/32/verify_32_api.c \
           crypto_verify/64/ref/verify_64.c \
           crypto_verify/64/verify_64_api.c \
           randombytes/nativeclient/randombytes_nativeclient.c \
           randombytes/randombytes.c \
           sodium/core.c \
           sodium/runtime.c \
           sodium/utils.c \
           sodium/version.c
SRC_C   += randombytes_salsa20_jitterentropy.c

vpath randombytes_salsa20_jitterentropy.c $(REP_DIR)/src/lib/libsodium
vpath %.cc $(REP_DIR)/src/lib/libsodium
vpath %.c  $(LIBSODIUM_SRC_DIR)
