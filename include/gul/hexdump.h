/**
 * \file   hexdump.h
 * \author \ref contributors
 * \date   Created on September 25, 2018
 * \brief  Declarations of hexdump()
 *
 * \copyright Copyright 2018 Deutsches Elektronen-Synchrotron (DESY), Hamburg
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

#pragma once

#include <iomanip>
#include <limits>
#include <sstream>
#include <type_traits>

namespace gul {

namespace detail{

// Small return type wrapper to improve usability with ostreams
// together with the following operator function
struct HexdumpOut {
    std::stringstream stream;
    operator std::string() {
        return stream.str();
    }
    std::string str() {
        return stream.str();
    }
};

std::ostream& operator<< (std::ostream& os, const HexdumpOut& hdo) {
    return os << hdo.stream.str();
}

// Helper to identify types that have data() and size()
// Until we have concepts ;-)
template <typename T, typename = int>
struct IsHexDumpContainer : std::false_type { };

template <typename T>
struct IsHexDumpContainer <T, decltype(std::declval<T>().data(),
                                       std::declval<T>().size(),
                                       0)> : std::true_type { };

} // namespace detail

template<typename ElemT>
::gul::detail::HexdumpOut hexdump(const ElemT* const buf, const size_t buflen, const std::string& prompt = "")
{
    const auto maxelem = 1000ul * 16; // 1000 lines with 16 elements each
    const auto maxidx = buflen < maxelem ? buflen : maxelem;
    // Get the number of hex digits to represent any value of a given integral type ElemT
    const auto nod = sizeof(ElemT) * 2;

    std::string indent(prompt.length(), ' ');
    std::string empty(nod + 1, ' ');
    ::gul::detail::HexdumpOut out;
    out.stream << std::hex << std::setfill('0');

    // Inspired by epatel @ stack overflow
    // https://stackoverflow.com/a/29865
    size_t i, j;
    for (i = 0; i < maxidx; i += 16) {
        out.stream << (i ? indent : prompt) << std::setw(6) << i << ": ";
        for (j = 0; j < 16; ++j) {
            if (i + j < maxidx) {
                long long ch = reinterpret_cast<typename std::make_unsigned<const ElemT>::type &>(buf[i+j]);
                out.stream << std::setw(nod) << ch << ' ';
            } else {
                out.stream << empty;
            }
        }
        out.stream << ' ';
        if (nod < 2) { // only char
            for (j = 0; j < 16; j++) {
                if (i + j < buflen)
                    out.stream << (isprint(buf[i + j]) ? buf[i + j] : '.');
            }
        }
        out.stream << "\n";
    }
    return out;
}

template<typename ContainerT,
    typename = std::enable_if_t<detail::IsHexDumpContainer<ContainerT>::value>,
    typename = std::enable_if_t<not std::is_convertible<ContainerT, string_view>::value> >
::gul::detail::HexdumpOut hexdump(const ContainerT& str, const std::string& prompt = "")
{
    return hexdump(str.data(), str.size(), prompt);
}

template<typename StringT,
    typename = std::enable_if_t<std::is_convertible<StringT, string_view>::value> >
::gul::detail::HexdumpOut hexdump(StringT str, const std::string& prompt = "")
{
    const auto sv = string_view{ str };
    return hexdump(sv.data(), sv.size(), prompt);
}

} // namespace gul
