//
// Strelka - Small Variant Caller
// Copyright (c) 2009-2017 Illumina, Inc.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//

///
/// \author Chris Saunders
///

#pragma once
#include "json/json.h"
#include "errorAnalysis/SequenceErrorCounts.hh"


/// model data as a mixture of variants and a two stage error process
///
void
indelModelVariantAndBinomialMixtureError(
    const SequenceErrorCounts& counts);

void
indelModelVariantAndBinomialMixtureErrorSimple(
        const SequenceErrorCounts& counts,
        const std::string& thetaFilename,
        const std::string& outputFilename);

double importLogTheta(
        std::string filename);

// move these to a more appropriate place later
// TODO: these classes can be automatically serialized with cereal
class IndelMotifBinomialMixture
{
public:
    unsigned repeatPatternSize = 0;
    unsigned repeatCount = 0;
    double indelRate = 0;
    double noisyLocusRate = 0;
};

class IndelModelBinomialMixture
{
public:
    std::vector<IndelMotifBinomialMixture> motifs;
    double theta = 0;
};

class IndelModelJson
{
public:
    IndelModelBinomialMixture model;

    void addMotif(
            unsigned repeatPatternSize,
            unsigned repeatCount,
            double indelRate,
            double noisyLocusRate);

    void exportIndelErrorModelToJsonFile(
            std::string filename);

    Json::Value
    generateMotifsNode();

};