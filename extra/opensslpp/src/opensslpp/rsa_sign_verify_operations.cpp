/* Copyright (c) 2022 Percona LLC and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; version 2 of
   the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include <cassert>
#include <vector>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>

#include <opensslpp/rsa_sign_verify_operations.hpp>

#include <opensslpp/core_error.hpp>
#include <opensslpp/rsa_key.hpp>

#include "opensslpp/rsa_key_accessor.hpp"

namespace opensslpp {

std::string sign_with_rsa_private_key(const std::string &digest_type,
                                      std::string_view digest_data,
                                      const rsa_key &key) {
  assert(!key.is_empty());

  if (!key.is_private())
    throw core_error{"RSA key does not have private components"};

  auto md = EVP_get_digestbyname(digest_type.c_str());
  if (md == nullptr) throw core_error{"unknown digest name"};

  auto md_nid = EVP_MD_type(md);

  std::string res(key.get_size_in_bytes(), '\0');

  unsigned int signature_length = 0;
  auto sign_status = RSA_sign(
      md_nid, reinterpret_cast<const unsigned char *>(digest_data.data()),
      digest_data.size(), reinterpret_cast<unsigned char *>(res.data()),
      &signature_length, rsa_key_accessor::get_impl_const_casted(key));

  if (sign_status != 1)
    core_error::raise_with_error_string(
        "cannot sign message digest with the specified private RSA key");

  res.resize(signature_length);
  return res;
}

bool verify_with_rsa_public_key(const std::string &digest_type,
                                std::string_view digest_data,
                                std::string_view signature_data,
                                const rsa_key &key) {
  assert(!key.is_empty());

  auto md = EVP_get_digestbyname(digest_type.c_str());
  if (md == nullptr) throw core_error{"unknown digest name"};

  auto md_nid = EVP_MD_type(md);

  auto verify_status = RSA_verify(
      md_nid, reinterpret_cast<const unsigned char *>(digest_data.data()),
      digest_data.size(),
      reinterpret_cast<const unsigned char *>(signature_data.data()),
      signature_data.size(), rsa_key_accessor::get_impl_const_casted(key));

  // RSA_verify() does not distinguish between "an error occurred" and
  // "invalid signature" - in both cases 0 is returned.
  // Therefore, we need to make sure that the OpenSSL error code queue
  // will be empty after this call, so that it would not affect invoking
  // code that may rely on ERR_get_error() / ERR_peek_error()
  if (verify_status == 0) ERR_clear_error();

  return verify_status == 1;
}

}  // namespace opensslpp
