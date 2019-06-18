/**
 * \file   test_SlidingBuffer.cc
 * \author \ref contributors
 * \date   Created on Feb 7, 2019
 * \brief  Test suite for the SlidingBuffer{}
 *
 * \copyright Copyright 2019 Deutsches Elektronen-Synchrotron (DESY), Hamburg
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 2.1 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "catch.h"
#include <gul.h>
#include <sstream>
#include <random>

namespace {

// A dummy struct for tests with nontrivial elements.
struct MyStruct {
    int a;
    std::string b;
};

// A SlidingBuffer variant that allows to dump the underlying container
// and the state of the buffer all in one to a stream.
// We use this to inspect the operations, if they handled internally as expected.
template<typename ElementT, std::size_t fixed_capacity = 0u,
    typename Container = typename std::conditional_t<(fixed_capacity >= 1u),
        std::array<ElementT, fixed_capacity>,
        std::vector<ElementT>>
    >
class SlidingBufferDebug : public gul::SlidingBuffer<ElementT, fixed_capacity, Container> {
public:
    using gul::SlidingBuffer<ElementT, fixed_capacity, Container>::SlidingBuffer;
    using typename gul::SlidingBuffer<ElementT, fixed_capacity, Container>::size_type;

    auto debugdump(std::ostream& s) -> std::ostream&
    {
        auto const len = this->capacity();
        for (size_type i{0}; i < len; ++i) {
            s << this->storage_.at(i);
            if (i == this->idx_end_)
                s << "* ";
            else
                s << "  ";
        }
        return s << '\n';
    }
};

template<typename ElementT, std::size_t fixed_capacity = 0u,
    typename Container = typename std::conditional_t<(fixed_capacity >= 1u),
        std::array<ElementT, fixed_capacity>,
        std::vector<ElementT>>
    >
class SlidingBufferExposedDebug : public gul::SlidingBufferExposed<ElementT, fixed_capacity, Container> {
public:
    using gul::SlidingBufferExposed<ElementT, fixed_capacity, Container>::SlidingBufferExposed;
    using typename gul::SlidingBufferExposed<ElementT, fixed_capacity, Container>::size_type;

    auto debugdump(std::ostream& s) -> std::ostream&
    {
        auto const len = this->capacity();
        for (size_type i{0}; i < len; ++i) {
            s << this->storage_.at(i);
            if (i == this->idx_end_)
                s << "* ";
            else
                s << "  ";
        }
        return s << '\n';
    }
};

auto random_double() -> double
{
    using bit_type = int64_t;

    union convert {
        decltype(random_double()) f;
        bit_type i;
    };
    auto converter = convert{};

    static_assert(sizeof(decltype(std::declval<convert>().f))
                <= sizeof(decltype(std::declval<convert>().i)),
                "Integer random buffer too small");
    static_assert(offsetof(convert, f) == offsetof(convert, i),
                "Alignment problem in conversion union");

    static std::random_device rd;
    auto static gen = std::mt19937{ rd() };
    auto static dis = std::uniform_int_distribution<bit_type>{ };

    do
        converter.i = dis(gen);
    while (std::isnan(converter.f)); // discard/retry if random number is NaN

    return converter.f;
}

template <typename DataT, typename StateT>
struct TestElement {
    using has_state = std::true_type;
    using data_type = DataT;
    using state_type = StateT;
    DataT val{ std::numeric_limits<DataT>::quiet_NaN() };
    StateT sta{ 0 };

    auto friend operator<< (std::ostream& s, const TestElement<DataT, StateT>& e) -> std::ostream&
    {
        return s << e.val;
    }
};

template <unsigned int buffer_size, typename T>
void do_queueing_tests(T& buff)
{
    auto const input_size = 100u;
    static_assert(input_size % buffer_size != 0,
            "We want to have unaligned sizes");

    auto nums = std::vector<double>(input_size);
    std::transform(nums.begin(), nums.end(), nums.begin(),
            [](auto) { return random_double(); } );

    for (auto el : nums)
        buff.push_front({ el, 0 });

    // Check last put element
    REQUIRE(buff.front().val == nums.at(nums.size() - 1));

    // Stuff Elements in and see if outcoming elements are as expected
    auto const signature = 1.234E5;
    for (auto i = 0u; i < buffer_size; ++i) {
        REQUIRE(buff.back().val == nums.at(nums.size() - buffer_size + i));
        buff.push_front({signature, 0u});
        REQUIRE(buff.front().val == signature);
    }
    // Now zero elements should come out...
    static_assert(buffer_size > 3, "Fifo too small for test case");
    REQUIRE(buff.back().val == signature);
    buff.push_front({0.0, 0u});
    REQUIRE(buff.back().val == signature);
    buff.push_front({0.0, 0u});
    REQUIRE(buff.back().val == signature);
    buff.push_front({0.0, 0u});
}

template <typename T>
void do_dumping_tests(T& buff)
{
    std::stringstream s1{ };

    buff.debugdump(s1);
    REQUIRE_THAT(s1.str(), Catch::Matchers::StartsWith(
                "nan* nan  nan  nan  nan  nan  nan  nan  nan  nan",
                Catch::CaseSensitive::No));

    for (auto i = 0; i < 15; ++i)
        buff.push_front({ static_cast<double>(i)*10 + static_cast<double>(i)/10, static_cast<unsigned int>(i) });

    std::stringstream s2{ };
    s2 << buff;
    REQUIRE_THAT(gul::trim(s2.str()), Catch::Matchers::Matches(
                "141.4  131.3  121.2  111.1  101  90.9  80.8  70.7  60.6  50.5"));
/*    std::stringstream s3{ };
    buff.debugdump(s3);
    REQUIRE(gul::trim(s3.str()) ==
                "101  111.1  121.2  131.3  141.4  50.5* 60.6  70.7  80.8  90.9");*/
}

} // anonymous namespace


TEST_CASE("SlidingBuffer test", "[SlidingBuffer]")
{
    SECTION("queueing tests") {
        auto constexpr buffer_size = 12u;

        auto buff_array = gul::SlidingBuffer<TestElement<double, unsigned int>, buffer_size>{};
        do_queueing_tests<buffer_size>(buff_array);

        auto buff_vector = gul::SlidingBuffer<TestElement<double, unsigned int>, 0>{};
        buff_vector.resize(buffer_size);
        do_queueing_tests<buffer_size>(buff_vector);
    }
    SECTION("dumping tests") {
        auto constexpr buffer_size = 10u;
        auto buff_array = SlidingBufferDebug<TestElement<double, unsigned int>, 10>{};
        do_dumping_tests(buff_array);

        auto buff_vector = SlidingBufferDebug<TestElement<double, unsigned int>, 0>{};
        buff_vector.resize(buffer_size);
        do_dumping_tests(buff_vector);
    }
    SECTION("vector construction tests") {
        auto buff1 = gul::SlidingBuffer<TestElement<double, unsigned int>, 0>{};
        REQUIRE(buff1.capacity() == 0);
        buff1.resize(10);
        REQUIRE(buff1.capacity() == 10);
        auto buff2 = gul::SlidingBuffer<TestElement<double, unsigned int>>{};
        REQUIRE(buff2.capacity() == 0);
        auto buff3 = gul::SlidingBuffer<TestElement<double, unsigned int>>{ 11 };
        REQUIRE(buff3.capacity() == 11);
        auto buff4 = gul::SlidingBuffer<TestElement<double, unsigned int>>(12);
        REQUIRE(buff4.capacity() == 12);
    }
    SECTION("iterator tests") {
        auto buff1 = SlidingBufferDebug<TestElement<double, unsigned int>>{ 10 };
        do_dumping_tests(buff1); // fill with stuff
        auto it12 = buff1.begin();
        decltype(it12) it{ it12 }; // copy ctor
        auto end = buff1.end();
        auto i = 0;
        for (; it != end; ++it, ++i) {
            auto ref = buff1.at(i).val;
            REQUIRE(buff1[i].val == ref);
            REQUIRE((*it).val == ref);
            REQUIRE(it->val == ref);
        }
        REQUIRE(i == 10);

        auto it2 = buff1.rbegin();
        auto end2 = buff1.rend();
        end2 = buff1.rend();
        i = 0;
        for (; it2 != end2; ++it2, ++i) {
            auto ref = buff1.at(9-i).val;
            REQUIRE((*it2).val == ref);
        }
        REQUIRE(i == 10);

        auto j = 0;
        for (auto const& e : buff1) {
            REQUIRE(e.val == buff1[j++].val);
        }

        auto x = buff1.begin();
        x->val = 1;
        REQUIRE(buff1.front().val == 1);
        x->val = 2;
        REQUIRE(buff1.front().val == 2);

        auto buff2 = gul::SlidingBuffer<int, 10>{ };
        buff2.push_front(0);
        buff2.push_front(1);
        buff2.push_front(2);
        buff2.push_front(3);
        auto k = 3;
        for (auto const& e : buff2)
            REQUIRE(e == k--);
        REQUIRE(k == -1);

        buff2.push_front(4);
        buff2.push_front(5);
        buff2.push_front(6);
        buff2.push_front(7);
        buff2.push_front(8);
        k = 8;
        for (auto const& e : buff2)
            REQUIRE(e == k--);
        REQUIRE(k == -1);

        buff2.push_front(9);
        k = 9;
        for (auto const& e : buff2)
            REQUIRE(e == k--);
        REQUIRE(k == -1);

        buff2.push_front(10);
        k = 10;
        for (auto const& e : buff2)
            REQUIRE(e == k--);
        REQUIRE(k == 0);

        buff2.push_front(11);
        buff2.push_front(12);
        buff2.push_front(13);
        k = 13;
        for (auto const& e : buff2)
            REQUIRE(e == k--);
        REQUIRE(k == 3);
    }
}

TEST_CASE("SlidingBuffer resize", "[SlidingBuffer]")
{
    SECTION("filling pattern 1") {
        // Create buffer with 5 places, filled with 7 elements
        // sectioning is near the middle
        //
        // Change size 5 -> 8 -> 11 -> 6 -> 5 -> 3
        // Change size 5 -> 4
        auto buff = SlidingBufferDebug<int, 0>{};
        buff.resize(5);
        buff.push_front(6);
        buff.push_front(6);
        buff.push_front(6);
        buff.push_front(7);
        buff.push_front(8);
        buff.push_front(9);
        buff.push_front(10);
        REQUIRE(buff.filled() == true);
        REQUIRE(buff.size() == 5);
        REQUIRE(buff.capacity() == 5);
        std::stringstream s{ };
        s << buff;
        REQUIRE_THAT(gul::trim(s.str()), Catch::Matchers::Matches(
            "10  9  8  7  6"));
        s.str("");
/*        buff.debugdump(s);
        REQUIRE(gul::trim(s.str()) == "9  10  6* 7  8");*/
        auto buffa = buff;

        buff.resize(5);
        REQUIRE(buff.filled() == true);
        REQUIRE(buff.size() == 5);
        REQUIRE(buff.capacity() == 5);
        s.str("");
        s << buff;
        REQUIRE_THAT(gul::trim(s.str()), Catch::Matchers::Matches(
            "10  9  8  7  6"));
        /*s.str("");
        buff.debugdump(s);
        REQUIRE(gul::trim(s.str()) == "9  10  6* 7  8");*/

        buff.resize(8);
        REQUIRE(buff.filled() == false);
        REQUIRE(buff.size() == 5);
        REQUIRE(buff.capacity() == 8);
        s.str("");
        s << buff;
        REQUIRE_THAT(gul::trim(s.str()), Catch::Matchers::Matches(
            "10  9  8  7  6"));
/*        s.str("");
        buff.debugdump(s);
        REQUIRE(gul::trim(s.str()) == "6  7  8  9  10  0* 0  0");*/

        buff.resize(11);
        REQUIRE(buff.filled() == false);
        REQUIRE(buff.size() == 5);
        REQUIRE(buff.capacity() == 11);
        s.str("");
        s << buff;
        REQUIRE_THAT(gul::trim(s.str()), Catch::Matchers::Matches(
            "10  9  8  7  6"));
/*        s.str("");
        buff.debugdump(s);
        REQUIRE(gul::trim(s.str()) == "6  7  8  9  10  0* 0  0  0  0  0");*/
        REQUIRE(*buff.begin() == 10);

        buff.resize(6);
        REQUIRE(buff.filled() == false);
        REQUIRE(buff.size() == 5);
        REQUIRE(buff.capacity() == 6);
        s.str("");
        s << buff;
        REQUIRE_THAT(gul::trim(s.str()), Catch::Matchers::Matches(
            "10  9  8  7  6"));
/*        s.str("");
        buff.debugdump(s);
        REQUIRE(gul::trim(s.str()) == "6  7  8  9  10  0*");*/

        buff.resize(5);
        REQUIRE(buff.filled() == true);
        REQUIRE(buff.size() == 5);
        REQUIRE(buff.capacity() == 5);
        s.str("");
        s << buff;
        REQUIRE_THAT(gul::trim(s.str()), Catch::Matchers::Matches(
            "10  9  8  7  6"));
/*        s.str("");
        buff.debugdump(s);
        REQUIRE(gul::trim(s.str()) == "6* 7  8  9  10");*/

        buffa.resize(4);
        REQUIRE(buffa.filled() == true);
        REQUIRE(buffa.size() == 4);
        REQUIRE(buffa.capacity() == 4);
        s.str("");
        s << buffa;
        REQUIRE_THAT(gul::trim(s.str()), Catch::Matchers::Matches(
            "10  9  8  7"));
/*        s.str("");
        buffa.debugdump(s);
        REQUIRE(gul::trim(s.str()) == "7* 8  9  10");*/

        buff.resize(3);
        REQUIRE(buff.filled() == true);
        REQUIRE(buff.size() == 3);
        REQUIRE(buff.capacity() == 3);
        s.str("");
        s << buff;
        REQUIRE_THAT(gul::trim(s.str()), Catch::Matchers::Matches(
            "10  9  8"));
/*        s.str("");
        buff.debugdump(s);
        REQUIRE(gul::trim(s.str()) == "8* 9  10");*/

        buff.resize(0);
        REQUIRE(buff.filled() == false);
        REQUIRE(buff.size() == 0);
        REQUIRE(buff.capacity() == 0);
/*        buff.push_front(1);   This is now UB
        REQUIRE(buff.filled() == true);
        REQUIRE(buff.size() == 0);
        REQUIRE(buff.capacity() == 0); */
    }
    SECTION("filling pattern 2") {
        // Try a different filling pattern
        // Create buffer with 8 places, filled with 9 elements
        // sectioning is almost left edge
        //
        // Change size 8 -> 12 -> 12 -> 15
        // Change size 8 -> 14
        // Change size 8 -> 13
        // [...]
        // Change size 8 -> 1
        auto buff = SlidingBufferDebug<int, 0>{};
        buff.resize(8);
        buff.push_front(2);
        buff.push_front(3);
        buff.push_front(4);
        buff.push_front(5);
        REQUIRE(buff.front() == 5);
        REQUIRE(buff.back() == 2);
        buff.push_front(6);
        buff.push_front(7);
        buff.push_front(8);
        buff.push_front(9);
        buff.push_front(10);
        REQUIRE(buff.filled() == true);
        REQUIRE(buff.size() == 8);
        REQUIRE(buff.capacity() == 8);
        REQUIRE(buff.front() == 10);
        REQUIRE(buff.back() == 3);
        std::stringstream s{ };
        s << buff;
        REQUIRE_THAT(gul::trim(s.str()), Catch::Matchers::Matches(
            "10  9  8  7  6  5  4  3"));
/*        s.str("");
        buff.debugdump(s);
        REQUIRE(gul::trim(s.str()) == "10  3* 4  5  6  7  8  9");*/

        auto buffa = buff;

        buff.resize(12);
        REQUIRE(buff.filled() == false);
        REQUIRE(buff.size() == 8);
        REQUIRE(buff.capacity() == 12);
        s.str("");
        s << buff;
        REQUIRE_THAT(gul::trim(s.str()), Catch::Matchers::Matches(
            "10  9  8  7  6  5  4  3"));
/*        s.str("");
        buff.debugdump(s);
        REQUIRE(gul::trim(s.str()) == "3  4  5  6  7  8  9  10  0* 0  0  0");*/

        buff.resize(12);
        REQUIRE(buff.filled() == false);
        REQUIRE(buff.size() == 8);
        REQUIRE(buff.capacity() == 12);
        s.str("");
        s << buff;
        REQUIRE_THAT(gul::trim(s.str()), Catch::Matchers::Matches(
            "10  9  8  7  6  5  4  3"));
/*        s.str("");
        buff.debugdump(s);
        REQUIRE(gul::trim(s.str()) == "3  4  5  6  7  8  9  10  0* 0  0  0");*/

        buff.resize(15);
        REQUIRE(buff.filled() == false);
        REQUIRE(buff.size() == 8);
        REQUIRE(buff.capacity() == 15);
        s.str("");
        s << buff;
        REQUIRE_THAT(gul::trim(s.str()), Catch::Matchers::Matches(
            "10  9  8  7  6  5  4  3"));
/*        s.str("");
        buff.debugdump(s);
        REQUIRE(gul::trim(s.str()) == "3  4  5  6  7  8  9  10  0* 0  0  0  0  0  0");*/

        buff = buffa;
        buff.resize(14);
        s.str("");
        s << buff;
        REQUIRE_THAT(gul::trim(s.str()), Catch::Matchers::Matches(
            "10  9  8  7  6  5  4  3"));
/*        s.str("");
        buff.debugdump(s);
        REQUIRE(gul::trim(s.str()) == "3  4  5  6  7  8  9  10  0* 0  0  0  0  0");*/
        buff = buffa;
        buff.resize(13);
        s.str("");
        s << buff;
        REQUIRE_THAT(gul::trim(s.str()), Catch::Matchers::Matches(
            "10  9  8  7  6  5  4  3"));
/*        s.str("");
        buff.debugdump(s);
        REQUIRE(gul::trim(s.str()) == "3  4  5  6  7  8  9  10  0* 0  0  0  0");*/
        buff = buffa;
        buff.resize(12);
        s.str("");
        s << buff;
        REQUIRE_THAT(gul::trim(s.str()), Catch::Matchers::Matches(
            "10  9  8  7  6  5  4  3"));
/*        s.str("");
        buff.debugdump(s);
        REQUIRE(gul::trim(s.str()) == "3  4  5  6  7  8  9  10  0* 0  0  0");*/
        buff = buffa;
        buff.resize(11);
        s.str("");
        s << buff;
        REQUIRE_THAT(gul::trim(s.str()), Catch::Matchers::Matches(
            "10  9  8  7  6  5  4  3"));
/*        s.str("");
        buff.debugdump(s);
        REQUIRE(gul::trim(s.str()) == "3  4  5  6  7  8  9  10  0* 0  0");*/
        buff = buffa;
        buff.resize(10);
        s.str("");
        s << buff;
        REQUIRE_THAT(gul::trim(s.str()), Catch::Matchers::Matches(
            "10  9  8  7  6  5  4  3"));
/*        s.str("");
        buff.debugdump(s);
        REQUIRE(gul::trim(s.str()) == "3  4  5  6  7  8  9  10  0* 0");*/
        buff = buffa;
        buff.resize(9);
        s.str("");
        s << buff;
        REQUIRE_THAT(gul::trim(s.str()), Catch::Matchers::Matches(
            "10  9  8  7  6  5  4  3"));
/*        s.str("");
        buff.debugdump(s);
        REQUIRE(gul::trim(s.str()) == "3  4  5  6  7  8  9  10  0*");*/
        buff = buffa;
        buff.resize(8); // no change
        s.str("");
        s << buff;
        REQUIRE_THAT(gul::trim(s.str()), Catch::Matchers::Matches(
            "10  9  8  7  6  5  4  3"));
/*        s.str("");
        buff.debugdump(s);
        REQUIRE(gul::trim(s.str()) == "10  3* 4  5  6  7  8  9");*/
        buff = buffa;
        buff.resize(7);
        s.str("");
        s << buff;
        REQUIRE_THAT(gul::trim(s.str()), Catch::Matchers::Matches(
            "10  9  8  7  6  5  4"));
/*        s.str("");
        buff.debugdump(s);
        REQUIRE(gul::trim(s.str()) == "4* 5  6  7  8  9  10");*/
        buff = buffa;
        buff.resize(6);
        s.str("");
        s << buff;
        REQUIRE_THAT(gul::trim(s.str()), Catch::Matchers::Matches(
            "10  9  8  7  6  5"));
/*        s.str("");
        buff.debugdump(s);
        REQUIRE(gul::trim(s.str()) == "5* 6  7  8  9  10");*/
        buff = buffa;
        buff.resize(5);
        s.str("");
        s << buff;
        REQUIRE_THAT(gul::trim(s.str()), Catch::Matchers::Matches(
            "10  9  8  7  6"));
/*        s.str("");
        buff.debugdump(s);
        REQUIRE(gul::trim(s.str()) == "6* 7  8  9  10");*/
        buff = buffa;
        buff.resize(4);
        s.str("");
        s << buff;
        REQUIRE_THAT(gul::trim(s.str()), Catch::Matchers::Matches(
            "10  9  8  7"));
/*        s.str("");
        buff.debugdump(s);
        REQUIRE(gul::trim(s.str()) == "7* 8  9  10");*/
        buff = buffa;
        buff.resize(3);
        s.str("");
        s << buff;
        REQUIRE_THAT(gul::trim(s.str()), Catch::Matchers::Matches(
            "10  9  8"));
/*        s.str("");
        buff.debugdump(s);
        REQUIRE(gul::trim(s.str()) == "8* 9  10");*/
        buff = buffa;
        buff.resize(2);
        s.str("");
        s << buff;
        REQUIRE_THAT(gul::trim(s.str()), Catch::Matchers::Matches(
            "10  9"));
/*        s.str("");
        buff.debugdump(s);
        REQUIRE(gul::trim(s.str()) == "9* 10");*/
        buff = buffa;
        buff.resize(1);
        s.str("");
        s << buff;
        REQUIRE_THAT(gul::trim(s.str()), Catch::Matchers::Matches(
            "10"));
/*        s.str("");
        buff.debugdump(s);
        REQUIRE(gul::trim(s.str()) == "10*");*/
    }
    SECTION("filling pattern 3") {
        // Try a different filling pattern
        // Create buffer with 8 places, filled with 15 elements
        // sectioning is almost right edge
        //
        // Change size 8 -> 12
        // Change size 8 -> 5
        // Change size 8 -> 4
        auto buff = SlidingBufferDebug<int, 0>(8);
        buff.push_front(3);
        buff.push_front(3);
        buff.push_front(3);
        buff.push_front(3);
        buff.push_front(3);
        buff.push_front(3);
        buff.push_front(3);
        buff.push_front(3);
        buff.push_front(4);
        buff.push_front(5);
        buff.push_front(6);
        buff.push_front(7);
        buff.push_front(8);
        buff.push_front(9);
        buff.push_front(10);
        REQUIRE(buff.filled() == true);
        REQUIRE(buff.size() == 8);
        REQUIRE(buff.capacity() == 8);
        std::stringstream s{ };
        s << buff;
        REQUIRE_THAT(gul::trim(s.str()), Catch::Matchers::Matches(
            "10  9  8  7  6  5  4  3"));
/*        s.str("");
        buff.debugdump(s);
        REQUIRE(gul::trim(s.str()) == "4  5  6  7  8  9  10  3*");*/

        auto buffa = buff;
        buff.resize(12);
        s.str("");
        s << buff;
        REQUIRE_THAT(gul::trim(s.str()), Catch::Matchers::Matches(
            "10  9  8  7  6  5  4  3"));
/*        s.str("");
        buff.debugdump(s);
        REQUIRE(gul::trim(s.str()) == "3  4  5  6  7  8  9  10  0* 0  0  0");*/

        buff = buffa;
        buff.resize(5);
        s.str("");
        s << buff;
        REQUIRE_THAT(gul::trim(s.str()), Catch::Matchers::Matches(
            "10  9  8  7  6"));
/*        s.str("");
        buff.debugdump(s);
        REQUIRE(gul::trim(s.str()) == "6* 7  8  9  10");*/

        buff = buffa;
        buff.resize(4);
        s.str("");
        s << buff;
        REQUIRE_THAT(gul::trim(s.str()), Catch::Matchers::Matches(
            "10  9  8  7"));
/*        s.str("");
        buff.debugdump(s);
        REQUIRE(gul::trim(s.str()) == "7* 8  9  10");*/
    }
}

TEST_CASE("SlidingBuffer: push_front(), empty(), size(), clear(), at() on array-based buffer",
          "[SlidingBuffer]")
{
    gul::SlidingBuffer<double, 2> buf;

    REQUIRE(buf.empty());
    REQUIRE(buf.size() == 0);
    REQUIRE_THROWS(buf.at(0));


    buf.push_front(1.0);
    REQUIRE(!buf.empty());
    REQUIRE(buf.size() == 1);
    REQUIRE(buf[0] == 1.0);

    buf.push_front(2.0);
    REQUIRE(!buf.empty());
    REQUIRE(buf.size() == 2);
    REQUIRE(buf[0] == 2.0);
    REQUIRE(buf[1] == 1.0);
    REQUIRE(buf.at(0) == 2.0);
    REQUIRE(buf.at(1) == 1.0);
    REQUIRE_THROWS(buf.at(2));
    REQUIRE_THROWS(buf.at(5));
    REQUIRE_THROWS(buf.at(-1));

    buf.push_front(3.0);
    REQUIRE(!buf.empty());
    REQUIRE(buf.size() == 2);
    REQUIRE(buf[0] == 3.0);
    REQUIRE(buf[1] == 2.0);

    buf.push_back(0.0);
    REQUIRE(buf.size() == 2);
    REQUIRE(buf[0] == 2.0);
    REQUIRE(buf[1] == 0.0);

    buf.clear();
    REQUIRE(buf.empty());
    REQUIRE(buf.size() == 0);
}

TEST_CASE("SlidingBuffer: push_back(), empty(), size(), clear(), at() on array-based buffer",
          "[SlidingBuffer]")
{
    gul::SlidingBuffer<double, 2> buf;

    REQUIRE(buf.empty());
    REQUIRE(buf.size() == 0);
    REQUIRE_THROWS(buf.at(0));


    buf.push_back(1.0);
    REQUIRE(!buf.empty());
    REQUIRE(buf.size() == 1);
    REQUIRE(buf[0] == 1.0);

    buf.push_back(2.0);
    REQUIRE(!buf.empty());
    REQUIRE(buf.size() == 2);
    REQUIRE(buf[0] == 1.0);
    REQUIRE(buf[1] == 2.0);
    REQUIRE(buf.at(0) == 1.0);
    REQUIRE(buf.at(1) == 2.0);
    REQUIRE_THROWS(buf.at(2));
    REQUIRE_THROWS(buf.at(5));
    REQUIRE_THROWS(buf.at(-1));

    buf.push_back(3.0);
    REQUIRE(!buf.empty());
    REQUIRE(buf.size() == 2);
    REQUIRE(buf[0] == 2.0);
    REQUIRE(buf[1] == 3.0);

    buf.push_front(0.0);
    REQUIRE(buf.size() == 2);
    REQUIRE(buf[0] == 0.0);
    REQUIRE(buf[1] == 2.0);

    buf.clear();
    REQUIRE(buf.empty());
    REQUIRE(buf.size() == 0);
}

TEST_CASE("SlidingBuffer: empty(), clear() on vector-based buffer", "[SlidingBuffer]")
{
    const gul::SlidingBuffer<int> buf{};
    REQUIRE(buf.empty()); // can be called in a const context

    gul::SlidingBuffer<int> buf2;
    buf2.resize(2);
    REQUIRE(buf2.empty());

    buf2.push_front(1);
    REQUIRE(!buf2.empty());

    buf2.clear();
    REQUIRE(buf2.empty());

    buf2.push_back(1);
    REQUIRE(!buf2.empty());
}

TEST_CASE("SlidingBuffer copying and moving", "[SlidingBuffer]")
{
    auto buffer = gul::SlidingBuffer<TestElement<double, unsigned int>>(6);
    for (auto i = 0u; i < 20; ++i)
        buffer.push_front({ 100.0 + 1.0 * i, i });

    auto it = buffer.begin();
    REQUIRE(buffer.at(0).val == 119.0);
    REQUIRE(it++->val == 119.0);
    REQUIRE(buffer.at(1).val == 118.0);
    REQUIRE(it++->val == 118.0);
    REQUIRE(buffer.at(2).val == 117.0);
    REQUIRE(it++->val == 117.0);
    REQUIRE(buffer.at(3).val == 116.0);
    REQUIRE(it++->val == 116.0);
    REQUIRE(buffer.at(4).val == 115.0);
    REQUIRE(it++->val == 115.0);
    REQUIRE(buffer.at(5).val == 114.0);
    REQUIRE(it++->val == 114.0);
    REQUIRE_THROWS(buffer.at(6).val);
    REQUIRE(it == buffer.end());

    REQUIRE(buffer.size() == 6);
    REQUIRE(buffer.filled() == true);

    SECTION("copy assignment") {
        decltype(buffer) buf_copy{};
        buf_copy = buffer;
        REQUIRE(buffer.at(3).val == 116.0);
        REQUIRE(buf_copy.at(3).val == 116.0);
        buf_copy[3].val = 50.0;
        REQUIRE(buffer.at(3).val == 116.0);
        REQUIRE(buf_copy.at(3).val == 50.0);
        buf_copy[4] = decltype(buf_copy)::value_type{ 66.0, 77 };
        REQUIRE(buffer.at(4).val == 115.0);
        REQUIRE(buf_copy.at(4).val == 66.0);
    }
    SECTION("copy constructor") {
        auto buf_copy{ buffer };
        REQUIRE(buffer.at(3).val == 116.0);
        REQUIRE(buf_copy.at(3).val == 116.0);
        buf_copy[3].val = 50.0;
        REQUIRE(buffer.at(3).val == 116.0);
        REQUIRE(buf_copy.at(3).val == 50.0);
        buf_copy[4] = decltype(buf_copy)::value_type{ 66.0, 77 };
        REQUIRE(buffer.at(4).val == 115.0);
        REQUIRE(buf_copy.at(4).val == 66.0);
    }
    SECTION("move assignment") {
        auto original = buffer;
        auto original_address = std::addressof(original.at(3));
        decltype(original) new_original{};
        new_original = std::move(original);
        REQUIRE(std::addressof(new_original.at(3)) == original_address);
    }
    SECTION("move constructor") {
        auto original = buffer;
        auto original_address = std::addressof(original.at(3));
        auto new_original{ std::move(original) };
        REQUIRE(std::addressof(new_original.at(3)) == original_address);
    }
    SECTION("copy assign iterators") {
        auto it1 = buffer.begin();
        auto it2 = it1++;
        REQUIRE(std::addressof(it1->val) != std::addressof(it2->val));
        it2 = it1;
        REQUIRE(std::addressof(it1->val) == std::addressof(it2->val));
    }
    SECTION("copy construct iterators") {
        auto it1 = buffer.begin();
        auto it2{ it1++ };
        REQUIRE(std::addressof(it1->val) != std::addressof(it2->val));
        it2 = it1;
        REQUIRE(std::addressof(it1->val) == std::addressof(it2->val));
    }
}

TEST_CASE("SlidingBufferExposed test", "[SlidingBuffer]")
{
    SECTION("queueing tests") {
        auto constexpr buffer_size = 12u;

        auto buff_array = gul::SlidingBufferExposed<TestElement<double, unsigned int>, buffer_size>{};
        do_queueing_tests<buffer_size>(buff_array);

        auto buff_vector = gul::SlidingBufferExposed<TestElement<double, unsigned int>, 0>{};
        buff_vector.resize(buffer_size);
        do_queueing_tests<buffer_size>(buff_vector);
    }
    SECTION("dumping tests") {
        auto constexpr buffer_size = 10u;
        auto buff_array = SlidingBufferExposedDebug<TestElement<double, unsigned int>, 10>{};
        do_dumping_tests(buff_array);

        auto buff_vector = SlidingBufferExposedDebug<TestElement<double, unsigned int>, 0>{};
        buff_vector.resize(buffer_size);
        do_dumping_tests(buff_vector);
    }
    SECTION("iterator tests") {
        auto buff1 = SlidingBufferExposedDebug<TestElement<double, unsigned int>>{ 10 };
        do_dumping_tests(buff1); // fill with stuff
        auto it12 = buff1.begin();
        decltype(it12) it{ it12 }; // copy ctor
        auto end = buff1.end();
        auto i = 0;
        auto iterator_data = std::vector<double>{ };
        for (; it != end; ++it, ++i) {
            auto ref = (*it).val;
            REQUIRE(it->val == ref);
            iterator_data.push_back(ref);
        }
        std::sort(iterator_data.begin(), iterator_data.end());

        REQUIRE(i == 10);
        auto index_data = std::vector<double>{ };
        for (; i--;) {
            auto ref = buff1.at(i).val;
            REQUIRE(buff1[i].val == ref);
            index_data.push_back(ref);
        }
        std::sort(index_data.begin(), index_data.end());
        REQUIRE(index_data == iterator_data);

        auto it2 = buff1.rbegin();
        auto end2 = buff1.rend();
        end2 = buff1.rend();
        i = 0;
        auto iterator2_data = std::vector<double>{ };
        for (; it2 != end2; ++it2, ++i) {
            iterator2_data.push_back((*it2).val);
        }
        std::sort(iterator2_data.begin(), iterator2_data.end());
        REQUIRE(i == 10);
        REQUIRE(index_data == iterator2_data);
    }
}

TEST_CASE("SlidingBuffer: push_front(const T&) with nontrivial T", "[SlidingBuffer]")
{
    MyStruct obj = { 1, "Hello" };

    gul::SlidingBuffer<MyStruct, 4> buf;

    buf.push_front(obj);

    REQUIRE(buf[0].a == 1);
    REQUIRE(buf[0].b == "Hello");
}

TEST_CASE("SlidingBuffer: push_front(T&&) with nontrivial T", "[SlidingBuffer]")
{
    MyStruct obj = { 1, "Hello" };

    gul::SlidingBuffer<MyStruct, 4> buf;

    buf.push_front(std::move(obj));

    REQUIRE(buf[0].a == 1);
    REQUIRE(buf[0].b == "Hello");
}

TEST_CASE("SlidingBuffer: push_back(const T&) with nontrivial T", "[SlidingBuffer]")
{
    MyStruct obj = { 1, "Hello" };

    gul::SlidingBuffer<MyStruct, 4> buf;

    buf.push_back(obj);

    REQUIRE(buf[0].a == 1);
    REQUIRE(buf[0].b == "Hello");
}

TEST_CASE("SlidingBuffer: push_back(T&&) with nontrivial T", "[SlidingBuffer]")
{
    MyStruct obj = { 1, "Hello" };

    gul::SlidingBuffer<MyStruct, 4> buf;

    buf.push_back(std::move(obj));

    REQUIRE(buf[0].a == 1);
    REQUIRE(buf[0].b == "Hello");
}


// vi:ts=4:sw=4:sts=4:et
