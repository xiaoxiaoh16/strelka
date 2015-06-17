// -*- mode: c++; indent-tabs-mode: nil; -*-
//
// Rumovsky
// Copyright (c) 2013-2014 Illumina, Inc.
//
// This software is provided under the terms and conditions of the
// Illumina Open Source Software License 1.
//
// You should have received a copy of the Illumina Open Source
// Software License 1 along with this program. If not, see
// <https://github.com/sequencing/licenses/>
//

///
/// \author Ole Schulz-Trieglaff
///

#pragma once

#include <vector>
#include <string>

#include "boost/unordered_map.hpp"

/// information added to each read in the process of assembly
///
struct AssemblyReadInfo
{
    AssemblyReadInfo() :
        isUsed(false)//,
        //contigId(0),
        //contigOffset(0)
    {}

    bool isUsed;
    //unsigned contigId; // index of the contig that this read is used in
    boost::unordered_map<unsigned,bool> contigIds;
    boost::unordered_map<unsigned,int> contigOffsets; // offset of the read w.r.t. to the contig (can be negative)
};

typedef std::pair<int,std::string> ReadAndPos;
typedef std::vector<ReadAndPos > AssemblyReadInput;
typedef std::vector<bool> AssemblyReadReversal;
typedef std::vector<AssemblyReadInfo> AssemblyReadOutput;
