// -*- mode: c++; indent-tabs-mode: nil; -*-
//
// Strelka - Small Variant Caller
// Copyright (c) 2009-2016 Illumina, Inc.
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
/// \author Mitch Bekritsky
///

#include "starling_common/indel_synchronizer.hh"

#include "blt_util/binomial_test.hh"
#include "blt_util/blt_exception.hh"
#include "blt_util/log.hh"
#include "calibration/IndelErrorModel.hh"

#include <cstdlib>

#include <iostream>
#include <sstream>



unsigned
indel_synchronizer::
register_sample(
    const depth_buffer &db,
    const depth_buffer &db2,
    const starling_sample_options &sample_opt,
    const double max_depth)
{
    assert(! _isFinalized);
    const unsigned sampleIndex(_indelSampleData.size());
    _indelSampleData.emplace_back(db,db2,sample_opt,max_depth);
    return sampleIndex;
}



std::pair<indel_synchronizer::iterator,indel_synchronizer::iterator>
indel_synchronizer::
pos_range_iter(
    const pos_t begin_pos,
    const pos_t end_pos)
{
    const indel_key end_range_key(end_pos);
    const iterator end(_indelBuffer.lower_bound(end_range_key));
    const indel_key begin_range_key(begin_pos-static_cast<pos_t>(_opt.max_indel_size));
    iterator begin(_indelBuffer.lower_bound(begin_range_key));
    for (; begin!=end; ++begin)
    {
        if (begin->first.right_pos() >= begin_pos) break;
    }
    return std::make_pair(begin,end);
}



// The goal is to return all indels with a left or right breakpoint in the
// range [begin_pos,end_pos]. Returning indels in addition to this set is
// acceptable.
//
// The indels_keys: "end_range_key" and "begin_range_key" take
// advantage of the indel NONE type, which sorts ahead of all other
// types at the same position.
//
std::pair<indel_synchronizer::const_iterator,indel_synchronizer::const_iterator>
indel_synchronizer::
pos_range_iter(
    const pos_t begin_pos,
    const pos_t end_pos) const
{
    const indel_key end_range_key(end_pos);
    const const_iterator end(_indelBuffer.lower_bound(end_range_key));
    const indel_key begin_range_key(begin_pos-static_cast<pos_t>(_opt.max_indel_size));
    const_iterator begin(_indelBuffer.lower_bound(begin_range_key));
    for (; begin!=end; ++begin)
    {
        if (begin->first.right_pos() >= begin_pos) break;
    }
    return std::make_pair(begin,end);
}



bool
indel_synchronizer::
insert_indel(
    const unsigned sampleId,
    const indel_observation& obs)
{
    assert(obs.key.type != INDEL::NONE);

    // if not previously observed
    iterator indelIter(_indelBuffer.find(obs.key));
    const bool isNovel(indelIter == _indelBuffer.end());
    if (isNovel)
    {
        const auto retval = _indelBuffer.insert(std::make_pair(obs.key,IndelData(getSampleCount(), obs.key)));
        indelIter = retval.first;
    }

    IndelData &id(get_indel_data(indelIter));
    id.addObservation(sampleId, obs.data);
    return isNovel;
}



bool
indel_synchronizer::
is_candidate_indel_impl_test_signal_noise(
    const indel_key& ik,
    const IndelData& id) const
{
    //
    // Step 1: find the error rate for this indel and context:
    //
    double indelToRefErrorProb(0.);
    {
        starling_indel_report_info iri;
        get_starling_indel_report_info(ik,id,_ref,iri);

        // refToIndelErrorProb does not factor in to this calculation, so put this in a temp value
        double refToIndelErrorProb(0.);
        _dopt.getIndelErrorModel().getIndelErrorRate(iri,refToIndelErrorProb,indelToRefErrorProb);
    }

    //
    // Step 2: determine if the observed counts are sig wrt the error rate for at least one sample:
    //
    const unsigned sampleCount(id.getSampleCount());
    for (unsigned sampleIndex(0); sampleIndex<sampleCount; ++sampleIndex)
    {
        const IndelSampleData& isd(id.getSampleData(sampleIndex));

        // for each sample, get the number of tier 1 reads supporting the indel
        // and the total number of tier 1 reads at this locus
        const unsigned tier1ReadSupportCount(isd.tier1_map_read_ids.size());
        unsigned totalReadCount(ebuff(sampleIndex).val(ik.pos-1));

        // total reads and indel reads are measured in different ways here, so the nonsensical
        // result of indel_reads>total_reads is possible. The total is fudged below to appear sane
        // before going into the count test:
        totalReadCount = std::max(totalReadCount,tier1ReadSupportCount);

        if (totalReadCount == 0) continue;

        // this min indel support coverage is applied regardless of error model:
        static const unsigned min_candidate_cov_floor(2);
        if (totalReadCount < min_candidate_cov_floor) continue;

        // test to see if the observed indel coverage has a binomial exact test
        // p-value above the rejection threshold. If this does not occur for the
        // counts observed in any sample, the indel cannot become a candidate
        if (_countCache.isRejectNull(totalReadCount, indelToRefErrorProb, tier1ReadSupportCount))
        {
            return true;
        }
    }
    return false;
}



bool
indel_synchronizer::
is_candidate_indel_impl_test_weak_signal(
    const IndelData& id) const
{
    const unsigned sampleCount(id.getSampleCount());
    for (unsigned sampleIndex(0); sampleIndex<sampleCount; ++sampleIndex)
    {
        const IndelSampleData& isd(id.getSampleData(sampleIndex));
        const unsigned tier1ReadSupportCount(isd.tier1_map_read_ids.size());
        static const unsigned min_candidate_cov_floor(1);
        if (tier1ReadSupportCount < min_candidate_cov_floor) continue;
        return true;
    }
    return false;
}



bool
indel_synchronizer::
is_candidate_indel_impl_test(
    const indel_key& ik,
    const IndelData& id) const
{
    // check whether the candidate has been externally specified:
    if (id.is_external_candidate) return true;

    if (_opt.is_candidate_indel_signal_test)
    {
        if (! is_candidate_indel_impl_test_signal_noise(ik,id)) return false;
    }
    else
    {
        if (! is_candidate_indel_impl_test_weak_signal(id)) return false;
    }

    /////////////////////////////////////////
    // test against short open-ended segments:
    //
    // call get_insert_size() instead of asking for the insert seq
    // so as to not finalize any incomplete insertions:
    if (ik.is_breakpoint() &&
        (_opt.min_candidate_indel_open_length > id.get_insert_size()))
    {
        return false;
    }

    /////////////////////////////////////////
    // test against max_depth:
    //
    const unsigned sampleCount(getSampleCount());
    for (unsigned sampleIndex(0); sampleIndex<sampleCount; ++sampleIndex)
    {
        const double max_depth(getIndelSampleData(sampleIndex).max_depth);
        if (max_depth <= 0.) continue;

        const unsigned estdepth(ebuff(sampleIndex).val(ik.pos-1));
        const unsigned estdepth2(ebuff2(sampleIndex).val(ik.pos-1));
        if ((estdepth+estdepth2) > max_depth) return false;
    }

    return true;
}



void
indel_synchronizer::
is_candidate_indel_impl(
    const indel_key& ik,
    const IndelData& id) const
{
    const bool is_candidate(is_candidate_indel_impl_test(ik,id));
    id.status.is_candidate_indel = is_candidate;
    id.status.is_candidate_indel_cached = true;
}



void
indel_synchronizer::
find_data_exception(const indel_key& ik) const
{
    std::ostringstream oss;
    oss << "ERROR: could not find indel_data for indel: " << ik;
    throw blt_exception(oss.str().c_str());
}



void
indel_synchronizer::
clear_pos(const pos_t pos)
{
    const iterator i_begin(pos_iter(pos));
    const iterator i_end(pos_iter(pos+1));
    _indelBuffer.erase(i_begin,i_end);
}



static
void
dump_range(
    indel_synchronizer::const_iterator i,
    const indel_synchronizer::const_iterator i_end,
    std::ostream& os)
{
    for (; i!=i_end; ++i)
    {
        os << "INDEL_KEY: " << i->first;
        os << "INDEL_DATA:\n";
        os << get_indel_data(i);
    }
}



void
indel_synchronizer::
dump_pos(
    const pos_t pos,
    std::ostream& os) const
{
    dump_range(pos_iter(pos),pos_iter(pos+1),os);
}



void
indel_synchronizer::
dump(std::ostream& os) const
{
    os << "INDEL_BUFFER DUMP ON\n";
    dump_range(_indelBuffer.begin(),_indelBuffer.end(),os);
    os << "INDEL_BUFFER DUMP OFF\n";
}
