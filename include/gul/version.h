/**
 * \file   version.h
 * \author \ref contributors
 * \date   Created on September 14, 2018
 * \brief  Provide information about the library version
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

#include "string_view.h"

namespace gul {

/**
 * Holds the git version tag of the sources that the library has
 * been build with.
 * It has the format returned by git describe --tags --always --dirty
 */
extern const string_view version_git;

/**
 * Holds the API version of the library.
 */
extern const string_view version_api;

} // namespace gul
