/**
 * \file    utility.h
 * \authors \ref contributors
 * \brief   Declaration of the in_place_t type and of the in_place constant.
 * \date    Created on March 31, 2023
 *
 * \copyright Copyright 2023 Deutsches Elektronen-Synchrotron (DESY), Hamburg
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

#ifndef GUL14_UTILITY_H_
#define GUL14_UTILITY_H_

namespace gul14 {

/// A type for constructor disambiguation, used by gul14::expected and gul14::optional.
struct in_place_t
{
    explicit in_place_t() = default;
};

/**
 * A tag that can be passed to the constructors of gul14::expected and gul14::optional
 * to request in-place construction.
 */
static constexpr in_place_t in_place{};

} // namespace gul14

#endif
