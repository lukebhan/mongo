/**
 *    Copyright (C) 2021-present MongoDB, Inc.
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the Server Side Public License, version 1,
 *    as published by MongoDB, Inc.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    Server Side Public License for more details.
 *
 *    You should have received a copy of the Server Side Public License
 *    along with this program. If not, see
 *    <http://www.mongodb.com/licensing/server-side-public-license>.
 *
 *    As a special exception, the copyright holders give permission to link the
 *    code of portions of this program with the OpenSSL library under certain
 *    conditions as described in each individual source file and distribute
 *    linked combinations including the program with the OpenSSL library. You
 *    must comply with the Server Side Public License in all respects for
 *    all of the code used other than as permitted herein. If you modify file(s)
 *    with this exception, you may extend this exception to your version of the
 *    file(s), but you are not obligated to do so. If you do not wish to do so,
 *    delete this exception statement from your version. If you delete this
 *    exception statement from all source files in the program, then also delete
 *    it in the license file.
 */

#include "mongo/platform/int128.h"
#include "mongo/unittest/unittest.h"

#include <boost/optional.hpp>

using namespace mongo;

void testValuesEqualUint128(const std::string &s, uint64_t hi, uint64_t lo){
  boost::optional<uint128_t> val = UInt128::MakeUint128FromString(s);
  ASSERT_FALSE(val == boost::none);
  ASSERT_TRUE(absl::Uint128High64(val.value()) ==  hi);
  ASSERT_TRUE(absl::Uint128Low64(val.value()) == lo);
}

void testValuesEqualInt128(const std::string &s, int64_t hi, uint64_t lo){
  boost::optional<int128_t> val = Int128::MakeInt128FromString(s);
  ASSERT_FALSE(val == boost::none);
  ASSERT_TRUE(absl::Int128High64(val.value()) ==  hi);
  ASSERT_TRUE(absl::Int128Low64(val.value()) == lo);
}

TEST(Uint128, BaseTest){
  testValuesEqualUint128("1234", 0, 1234);
}

TEST(Uint128, MaxUint) {
  // Max uint128 is all 1s for 128 values
  testValuesEqualUint128("340282366920938463463374607431768211455", 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF);
}

TEST(Uint128, MinUint) {
  testValuesEqualUint128("0", 0, 0);
}

TEST(Uint128, OutOfBounds){
  boost::optional<uint128_t> val = UInt128::MakeUint128FromString("340282366920938463463374607431768211456");
  ASSERT_TRUE(val == boost::none);
}

TEST(Uint128, EmptyString) {
  boost::optional<uint128_t> val = UInt128::MakeUint128FromString("");
  ASSERT_TRUE(val == boost::none);
}

TEST(Uint128, BadChar) {
  boost::optional<uint128_t> val = UInt128::MakeUint128FromString("234C");
  ASSERT_TRUE(val == boost::none);
}

TEST(Int128, BaseTest){
  testValuesEqualInt128("1234", 0, 1234);
}

TEST(Int128, BaseTestNegative){
  // 2's complement yeilds 1111111111111111111111111111111111111111111111111111101100101110
  testValuesEqualInt128("-1234", -1, 0xFFFFFFFFFFFFFB2E);
}

TEST(Int128, MaxInt) {
  // Max int128 is all 1s for 127 values
  testValuesEqualInt128("170141183460469231731687303715884105727", 0x7FFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF);
}

TEST(Int128, MinInt) {
  testValuesEqualInt128("-170141183460469231731687303715884105727", -0x8000000000000000, 0x1);
}

TEST(Int128, OutOfBounds){
  boost::optional<int128_t> val = Int128::MakeInt128FromString("-170141183460469231731687303715884105728");
  ASSERT_TRUE(val == boost::none);
}

TEST(Int128, EmptyString) {
  boost::optional<int128_t> val = Int128::MakeInt128FromString("");
  ASSERT_TRUE(val == boost::none);
}

TEST(Int128, BadChar) {
  boost::optional<int128_t> val = Int128::MakeInt128FromString("234C");
  ASSERT_TRUE(val == boost::none);
}
