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

#pragma once

#include <absl/numeric/int128.h>
#include <boost/optional.hpp>
#include <ctype.h>

using uint128_t = absl::uint128;
using int128_t = absl::int128;

// Wrapper class providing a static method to create a uint128 from string as this does not exist in
// native absl.
class UInt128 {
public:
    static boost::optional<uint128_t> MakeUint128FromString(const std::string& s) {
        if (s.empty())
            return boost::none;
        uint128_t startingVal = 0;
        uint128_t prevVal = 0;
        for (size_t index = 0; index < s.size(); ++index) {
            const char cur = s[index];
            if (!std::isdigit(cur))
                return boost::none;
            startingVal *= 10;
            startingVal += cur - '0';
            if (prevVal > startingVal)
                // Overflow
                return boost::none;
            prevVal = startingVal;
        }
        return startingVal;
    }
};

// Wrapper class providing a static method to create a int128 from string as this does not exist in
// native absl.
class Int128 {
public:
    static boost::optional<int128_t> MakeInt128FromString(const std::string& s) {
        if (s.empty())
            return boost::none;
        bool sign = false;
        boost::optional<uint128_t> val;
        if (s[0] == '-') {
            sign = true;
            // Skip the - sign.
            val = UInt128::MakeUint128FromString(s.substr(1, s.length() - 1));
        } else {
            val = UInt128::MakeUint128FromString(s);
        }
        if (val == boost::none || val.value() > absl::Int128Max())
            // overflow
            return boost::none;
        int128_t signedVal =
            sign ? static_cast<int128_t>(val.value()) * -1 : static_cast<int128_t>(val.value());
        return signedVal;
    }
};
