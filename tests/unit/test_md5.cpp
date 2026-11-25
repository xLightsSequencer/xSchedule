// MD5 is used for the web interface login. RFC 1321 test vectors.

#include <catch2/catch_test_macros.hpp>

#include "md5.h"

TEST_CASE("MD5 matches the RFC 1321 test suite", "[md5]") {
    CHECK(md5("") == "d41d8cd98f00b204e9800998ecf8427e");
    CHECK(md5("a") == "0cc175b9c0f1b6a831c399e269772661");
    CHECK(md5("abc") == "900150983cd24fb0d6963f7d28e17f72");
    CHECK(md5("message digest") == "f96b697d7cb7938d525a2f31aaf161d0");
    CHECK(md5("abcdefghijklmnopqrstuvwxyz") == "c3fcd3d76192e4007dfb496cca67e13b");
    CHECK(md5("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789") == "d174ab98d277d9f5a5611c2c9f419d9f");
    CHECK(md5("12345678901234567890123456789012345678901234567890123456789012345678901234567890") == "57edf4a22be3c955ac49da2e2107b67a");
}

TEST_CASE("MD5 incremental update matches one shot", "[md5]") {
    MD5 m;
    m.update("message ", 8);
    m.update("digest", 6);
    m.finalize();
    CHECK(m.hexdigest() == md5("message digest"));
}
