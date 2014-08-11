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
/*
 * calbrationmodels.cpp
 *
*  Created on: Oct 10, 2013
 * Author: Morten Kallberg
 */

#include "calibration_models.hh"
#include <cassert>
#include <exception>
#include <sstream>
#include <string>
#include <iostream>
#include <fstream>
#include <iterator>
#include <map>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <stdlib.h>     /* atof */


//#define DEBUG_CAL

#ifdef DEBUG_CAL
#include "blt_util/log.hh"
#endif

calibration_models::calibration_models()
{
//    this->set_model("DEFAULT");
}
calibration_models::~calibration_models() {}

int calibration_models::get_case_cutoff(CALIBRATION_MODEL::var_case my_case){
    return this->get_model(this->model_name).get_var_threshold(my_case);
}

bool calibration_models::is_current_logistic(){
    if (this->is_default_model)
        return false;
    return this->get_model(this->model_name).is_logitic_model();
}


void calibration_models::clasify_site(site_info& si)
{

    if (si.dgt.is_snp && !this->is_default_model)
    {
        featuremap features = si.get_qscore_features();     // create site value feature dict
        c_model myModel = this->get_model(this->model_name);
        myModel.score_instance(features,si);
    }
    else
    {
        // don't know what to do with this site, throw it to the old default filters
        this->default_clasify_site(si);
    }
}

void calibration_models::clasify_site(indel_info& ii)
{
    if ( (ii.iri.it==INDEL::INSERT || ii.iri.it==INDEL::DELETE) && !this->is_default_model)
    {
        featuremap features = ii.get_qscore_features();
        c_model myModel = this->get_model(this->model_name);
        myModel.score_instance(features,ii);
    }
    {
        this->default_clasify_site(ii);
    }
}


void calibration_models::default_clasify_site(site_info& si)
{
    if (this->opt->is_min_gqx)
    {
        if (si.smod.gqx<this->opt->min_gqx) si.smod.set_filter(VCF_FILTERS::LowGQX);
    }
    if (this->dopt->is_max_depth)
    {
        if ((si.n_used_calls+si.n_unused_calls) > this->dopt->max_depth) si.smod.set_filter(VCF_FILTERS::HighDepth);
    }
    // high DPFratio filter
    if (this->opt->is_max_base_filt)
    {
        const unsigned total_calls(si.n_used_calls+si.n_unused_calls);
        if (total_calls>0)
        {
            const double filt(static_cast<double>(si.n_unused_calls)/static_cast<double>(total_calls));
            if (filt>this->opt->max_base_filt) si.smod.set_filter(VCF_FILTERS::HighBaseFilt);
        }
    }
    if (si.dgt.is_snp)
    {
        if (this->opt->is_max_snv_sb)
        {
            if (si.dgt.sb>opt->max_snv_sb) si.smod.set_filter(VCF_FILTERS::HighSNVSB);
        }
        if (this->opt->is_max_snv_hpol)
        {
            if (static_cast<int>(si.hpol)>this->opt->max_snv_hpol) si.smod.set_filter(VCF_FILTERS::HighSNVHPOL);
        }
    }
}

// default rules based indel model
void calibration_models::default_clasify_site(indel_info& ii)
{
    if (ii.dindel.max_gt != ii.dindel.max_gt_poly)
    {
        ii.imod.gqx=0;
    }
    else
    {
        ii.imod.gqx=std::min(ii.dindel.max_gt_poly_qphred,ii.dindel.max_gt_qphred);
    }
    ii.imod.max_gt=ii.dindel.max_gt_poly;
    ii.imod.gq=ii.dindel.max_gt_poly_qphred;


    if (this->opt->is_min_gqx)
    {
        if (ii.imod.gqx<opt->min_gqx) ii.imod.set_filter(VCF_FILTERS::LowGQX);
    }

    if (this->dopt->is_max_depth)
    {
        if (ii.isri.depth > this->dopt->max_depth) ii.imod.set_filter(VCF_FILTERS::HighDepth);
    }

    if (this->opt->is_max_ref_rep)
    {
        if (ii.iri.is_repeat_unit)
        {
            if ((ii.iri.repeat_unit.size() <= 2) &&
                (static_cast<int>(ii.iri.ref_repeat_count) > this->opt->max_ref_rep))
            {
                ii.imod.set_filter(VCF_FILTERS::HighRefRep);
            }
        }
    }
}

void calibration_models::set_model(const std::string& name)
{
    modelmap::iterator it = this->models.find(boost::to_upper_copy(name));
    assert("Unrecognized calibration model given using --scoring-model option in set_model" && it != this->models.end());

    if (it != this->models.end())
    {
        this->model_name = boost::to_upper_copy(name);
        this->is_default_model = false;
    }
//    log_os << "Calibration model set to '" << this->model_name << "'\n";
//    log_os << "Calibration model is default '" << this->is_default_model << "'\n";
#ifdef DEBUG_CAL
    log_os << "Calibration model set to '" << this->model_name << "'\n";
#endif
}

c_model& calibration_models::get_model(std::string& name)
{
    modelmap::iterator it = this->models.find(boost::to_upper_copy(name));
    assert("Unrecognized calibration model given using --scoring-model option" && it != this->models.end());
    return this->models.find(boost::to_upper_copy(name))->second;
}

void calibration_models::add_model_pars(std::string& name,parmap& my_pars)
{
#ifdef DEBUG_CAL
    log_os << "Adding pars for model " << name << "\n";
    log_os << "Adding pars " << my_pars.size() << "\n";
#endif
    this->get_model(name).add_parameters(my_pars);
    my_pars.clear();
}


void calibration_models::load_models(std::string model_file)
{
    using namespace boost::algorithm;

#ifdef DEBUG_CAL
    log_os << "Loading models from file: " << model_file << "\n";
#endif
    std::ifstream myReadFile;
    myReadFile.open(model_file.c_str());
    std::string output;
    std::string parspace;
    std::string submodel;
    std::string current_name;
    parmap pars;
    if (myReadFile.is_open())
    {
        while (!myReadFile.eof())
        {
            std::getline (myReadFile,output);
            std::vector<std::string> tokens;
            split(tokens, output, is_any_of(" ")); // tokenize string
            //case new model
            if (tokens.at(0).substr(0,3)=="###")
            {
                if (pars.size()>0)
                {
                    this->add_model_pars(current_name,pars);
                }
                current_name = boost::to_upper_copy(tokens.at(1));
                c_model current_model(current_name,tokens.at(2));

                current_model.dopt = this->dopt;
                current_model.opt = this->opt;

                this->models.insert(modelmap::value_type(current_name, current_model));
#ifdef DEBUG_CAL
                log_os << "Loading model: " << tokens.at(1) << " Type: " << tokens.at(2) << "\n";
#endif
            }
            //load submodel
            else if (tokens.at(0)=="#")
            {
#ifdef DEBUG_CAL
                log_os << "submodel: " << tokens.at(1) << " parspace: " << tokens.at(2) << "\n";
#endif
                submodel = tokens.at(1);
                parspace = tokens.at(2);
            }
            //case load parameters
            else
            {
                if (tokens.size()>1)
                {
#ifdef DEBUG_CAL
                    log_os << " setting " << tokens.at(0) << " = " << tokens.at(1) << "\n";
#endif
                    pars[submodel][parspace][tokens.at(0)] = atof(tokens.at(1).c_str());
                }
            }
        }
        this->add_model_pars(current_name,pars);
    }
#ifdef DEBUG_CAL
    log_os << "Done loading models" << "\n";
#endif
    myReadFile.close();
}