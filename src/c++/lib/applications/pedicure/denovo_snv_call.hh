// -*- mode: c++; indent-tabs-mode: nil; -*-
//
// Starka
// Copyright (c) 2009-2014 Illumina, Inc.
//
// This software is provided under the terms and conditions of the
// Illumina Open Source Software License 1.
//
// You should have received a copy of the Illumina Open Source
// Software License 1 along with this program. If not, see
// <https://github.com/sequencing/licenses/>
//

///
/// \author Chris Saunders
///

#pragma once

#include <cstdint>


struct denovo_snv_call
{
    struct result_set
    {
        unsigned max_gt;
        int dsnv_qphred = 0;
    };

    bool
    is_snv() const
    {
        return (0 != rs.dsnv_qphred);
    }

    bool
    is_output() const
    {
        return (is_snv() || is_forced_output);
    }

    unsigned ref_gt = 0;
    uint8_t dsnv_tier = 0;
    bool is_forced_output = false;
    result_set rs;
};