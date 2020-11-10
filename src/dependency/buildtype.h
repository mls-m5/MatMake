/*
 * buildtype.h
 *
 *  Created on: 26 jan. 2020
 *      Author: Mattias Larsson Sköld
 */

#pragma once

enum BuildType {
    Executable,
    Test,
    Shared,
    Static,
    Copy,
    Object,
    FromTarget,
    NotSpecified
};
