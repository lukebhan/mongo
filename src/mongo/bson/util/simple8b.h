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

#include <array>
#include <deque>
#include <vector>

#include "mongo/bson/util/builder.h"
#include "mongo/platform/int128.h"

namespace mongo {

/**
 * Simple8bBuilder compresses a series of integers into chains of 64 bit Simple8b word.
 */
template <typename T>
class Simple8bBuilder {
public:
    // Number of different type of selectors and their extensions available
    static constexpr uint8_t kNumOfSelectorTypes = 4;

    // The min number of meaningful bits each selector can store
    static constexpr std::array<uint8_t, 4> kMinDataBits = {1, 2, 4, 4};

    // Callback to handle writing of finalized Simple-8b blocks. Machine Endian byte order.
    using WriteFn = std::function<void(uint64_t)>;
    Simple8bBuilder(WriteFn writeFunc);
    ~Simple8bBuilder();

    /**
     * Checks if we can append val to an existing RLE and handles the ending of a RLE.
     * The default RLE value at the beginning is 0.
     * Otherwise, Appends a value to the Simple8b chain of words.
     * Return true if successfully appended and false otherwise.
     */
    bool append(T val);

    /**
     * Appends an empty bucket to handle missing values. This works by incrementing an underlying
     * simple8b index by one and encoding a "missing value" in the simple8b block as all 1s.
     */
    void skip();

    /**
     * Stores all values for RLE or in _pendingValues into _buffered even if the Simple8b word will
     * not be opimtal and use a larger selector than necessary because we don't have enough integers
     * to use one wiht more slots.
     */
    void flush();

    /**
     * This stores a value that has yet to be added to the buffer. It also stores the number of bits
     * required to store the value for each selector extension type. Furthermore, it stores the
     * number of trailing zeros that would be stored if this value was stored according to the
     * respective selector type. The arrays are indexed using the same selector indexes as defined
     * in the cpp file.
     */
    struct PendingValue {
        PendingValue(T val,
                     std::array<uint8_t, kNumOfSelectorTypes> bitCount,
                     std::array<uint8_t, kNumOfSelectorTypes> trailingZerosCount,
                     bool skip);
        T val;
        std::array<uint8_t, kNumOfSelectorTypes> bitCount = {0, 0, 0, 0};
        // This is not the total number of trailing zeros, but the trailing zeros that will be
        // stored given the selector chosen.
        std::array<uint8_t, kNumOfSelectorTypes> trailingZerosCount = {0, 0, 0, 0};
        bool skip;
    };

private:
    /**
     * Appends a value to the Simple8b chain of words.
     * Return true if successfully appended and false otherwise.
     */
    bool _appendValue(T value, bool tryRle);

    /**
     * Appends a skip to _pendingValues and forms a new Simple8b word if there is no space.
     */
    void _appendSkip();

    /**
     * When an RLE ends because of inconsecutive values, check if there are enough
     * consecutive values for a RLE value and/or any values to be appended to _pendingValues.
     */
    void _handleRleTermination();

    /**
     * Based on _rleCount, create a RLE Simple8b word if possible.
     * If _rleCount is not large enough, do nothing.
     */
    void _appendRleEncoding();

    /*
     * Checks to see if RLE is possible and/or ongoing
     */
    bool _rlePossible() const;

    /**
     * Tests if a value would fit inside the current simple8b word using any of the selectors
     * selector. Returns true if adding the value fits in the current simple8b word and false
     * otherwise.
     */
    bool _doesIntegerFitInCurrentWord(const PendingValue& value);

    /*
     * This is a helper method for testing if a given selector will allow an integer to fit in a
     * simple8b word. Takes in a value to be stored and an extensionType representing the selector
     * compression method to check. Returns true if the word fits and updates the global
     * _lastValidExtensionType with the extensionType passed. If false, updates
     * isSelectorPossible[extensionType] to false so we do not need to recheck that extension if we
     * find a valid type and more values are added into the current word.
     */
    bool _doesIntegerFitInCurrentWordWithGivenSelectorType(const PendingValue& value,
                                                           uint8_t extensionType);

    /**
     * Encodes the largest possible simple8b word from _pendingValues without unused buckets using
     * the selector compression method passed in extensionType. Assumes is always called right after
     * _doesIntegerFitInCurrentWord fails for the first time. It removes the integers used to form
     * the simple8b word from _pendingValues permanently and updates our global state with any
     * remaining integers in _pendingValues.
     */
    int64_t _encodeLargestPossibleWord(uint8_t extensionType);

    /**
     * Takes a vector of integers to be compressed into a 64 bit word via the selector type given.
     * The values will be stored from right to left in little endian order.
     * For now, we will assume that all ints in the vector are greater or equal to zero.
     * We will also assume that the selector and all values will fit into the 64 bit word.
     * Returns the encoded Simple8b word if the inputs are valid and errCode otherwise.
     */
    template <typename Func>
    uint64_t _encode(Func func, uint8_t selectorIdx, uint8_t extensionType);

    /**
     * Updates the simple8b current state with the passed parameters. The maximum is always taken
     * between the current state and the new value passed. This is used to keep track of the size of
     * the simple8b word that we will need to encode.
     */
    void _updateSimple8bCurrentState(const PendingValue& val);

    // If RLE is ongoing, the number of consecutive repeats fo lastValueInPrevWord.
    uint32_t _rleCount = 0;
    // If RLE is ongoing, the last value in the previous Simple8b word.
    PendingValue _lastValueInPrevWord = {0, {0, 0, 0, 0}, {0, 0, 0, 0}, false};

    // These variables hold the max amount of bits for each value in _pendingValues. They are
    // updated whenever values are added or removed from _pendingValues to always reflect the max
    // value in the deque.
    std::array<uint8_t, kNumOfSelectorTypes> _currMaxBitLen = kMinDataBits;
    std::array<uint8_t, kNumOfSelectorTypes> _currTrailingZerosCount = {0, 0, 0, 0};

    // This holds the last valid selector compression method that succeded for
    // doesIntegerFitInCurrentWord and is used to designate the compression type when we need to
    // write a simple8b word to buffer.
    uint8_t _lastValidExtensionType = 0;

    // Holds whether the selector compression method is possible. This is updated in
    // doesIntegerFitInCurrentWordWithSelector to avoid unnecessary calls when a selector is already
    // invalid for the current set of words in _pendingValues.
    std::array<bool, kNumOfSelectorTypes> isSelectorPossible = {true, true, true, true};

    // This holds values that have not be encoded to the simple8b buffer, but are waiting for a full
    // simple8b word to be filled before writing to buffer.
    std::deque<PendingValue> _pendingValues;

    // User-defined callback to handle writing of finalized Simple-8b blocks
    WriteFn _writeFn;
};

/**
 * Simple8b provides an interface to read Simple8b encoded data built by Simple8bBuilder above
 */
template <typename T>
class Simple8b {
public:
    class Iterator {
    public:
        friend class Simple8b;

        // typedefs expected in iterators
        using iterator_category = std::forward_iterator_tag;
        using difference_type = ptrdiff_t;
        using value_type = boost::optional<T>;
        using pointer = const boost::optional<T>*;
        using reference = const boost::optional<T>&;

        /**
         * Returns the number of values in the current Simple8b block that the iterator is
         * positioned on.
         */
        size_t blockSize() const;

        /**
         * Returns the value in Little Endian byte order at the current iterator position.
         */
        pointer operator->() const {
            return &_value;
        }
        reference operator*() const {
            return _value;
        }

        /**
         * Advance the iterator one step.
         */
        Iterator& operator++();

        /**
         * Advance the iterator to the next Simple8b block.
         */
        Iterator& advanceBlock();

        bool operator==(const Iterator& rhs) const;
        bool operator!=(const Iterator& rhs) const;

    private:
        Iterator(const uint64_t* pos, const uint64_t* end);

        /**
         * Loads the current Simple8b block into the iterator
         */
        void _loadBlock();
        void _loadValue();

        /**
         * RLE count, may only be called if iterator is positioned on an RLE block
         */
        uint16_t _rleCountInCurrent(uint8_t selectorExtension) const;

        const uint64_t* _pos;
        const uint64_t* _end;

        // Current Simple8b block in native endian
        uint64_t _current;

        boost::optional<T> _value;

        // Mask for getting a single Simple-8b slot
        uint64_t _mask;

        // Remaining RLE count for repeating previous value
        uint16_t _rleRemaining;

        // Number of positions to shift the mask to get slot for current iterator position
        uint8_t _shift;

        // Number of bits in single Simple-8b slot, used to increment _shift when updating iterator
        // position
        uint8_t _bitsPerValue;

        // Variables for the extended Selectors 7 and 8 with embedded count in Simple-8b slot
        // Mask to extract count
        uint8_t _countMask;

        // Number of bits for the count
        uint8_t _countBits;

        // Multiplier of the value in count to get number of zeros
        uint8_t _countMultiplier;

        // Holds the current simple8b block's selector
        uint8_t _selector;

        // Holds the current simple8b blocks's extension type
        uint8_t _extensionType;
    };

    /**
     * Does not take ownership of buffer, must remain valid during the lifetime of this class.
     */
    Simple8b(const char* buffer, int size);

    /**
     * Forward iterators to read decompressed values
     */
    Iterator begin() const;
    Iterator end() const;

private:
    const char* _buffer;
    int _size;
};

}  // namespace mongo
