/******************************************************************************
 *   Copyright (c) 2012 Jan Rheinländer <jrheinlaender@users.sourceforge.net> *
 *                                                                            *
 *   This file is part of the FreeCAD CAx development system.                 *
 *                                                                            *
 *   This library is free software; you can redistribute it and/or            *
 *   modify it under the terms of the GNU Library General Public              *
 *   License as published by the Free Software Foundation; either             *
 *   version 2 of the License, or (at your option) any later version.         *
 *                                                                            *
 *   This library  is distributed in the hope that it will be useful,         *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            *
 *   GNU Library General Public License for more details.                     *
 *                                                                            *
 *   You should have received a copy of the GNU Library General Public        *
 *   License along with this library; see the file COPYING.LIB. If not,       *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,            *
 *   Suite 330, Boston, MA  02111-1307, USA                                   *
 *                                                                            *
 ******************************************************************************/


#include "PreCompiled.h"
#ifndef _PreComp_
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <Precision.hxx>
#endif

#include "FeatureMultiTransform.h"
#include "FeatureAddSub.h"
#include "FeatureScaled.h"


using namespace PartDesign;

namespace PartDesign
{


PROPERTY_SOURCE(PartDesign::MultiTransform, PartDesign::Transformed)

MultiTransform::MultiTransform()
{
    ADD_PROPERTY(Transformations, (nullptr));
    Transformations.setSize(0);
}

void MultiTransform::positionBySupport()
{
    PartDesign::Transformed::positionBySupport();
    std::vector<App::DocumentObject*> transFeatures = Transformations.getValues();
    for (auto f : transFeatures) {
        auto transFeature = freecad_cast<PartDesign::Transformed>(f);
        if (!transFeature) {
            throw Base::TypeError("Transformation features must be subclasses of Transformed");
        }

        transFeature->Placement.setValue(this->Placement.getValue());

        // To avoid that a linked transform feature stays touched after a recompute
        // we have to purge the touched state
        if (this->isRecomputing()) {
            transFeature->purgeTouched();
        }
    }
}

short MultiTransform::mustExecute() const
{
    if (Transformations.isTouched()) {
        return 1;
    }
    return Transformed::mustExecute();
}

const std::list<gp_Trsf>
MultiTransform::getTransformations(const std::vector<App::DocumentObject*> originals)
{
    std::vector<App::DocumentObject*> transFeatures = Transformations.getValues();

    gp_Pnt cog;
    if (!originals.empty()) {
        // Find centre of gravity of first original
        // FIXME: This method will NOT give the expected result for more than one original!
        if (auto addFeature =
                freecad_cast<PartDesign::FeatureAddSub>(originals.front())) {
            TopoDS_Shape original = addFeature->AddSubShape.getShape().getShape();

            GProp_GProps props;
            BRepGProp::VolumeProperties(original, props);
            cog = props.CentreOfMass();
        }
    }

    std::list<gp_Trsf> result;
    std::list<gp_Pnt> cogs;

    for (auto const& f : transFeatures) {
        auto transFeature = freecad_cast<PartDesign::Transformed>(f);
        if (!transFeature) {
            throw Base::TypeError("Transformation features must be subclasses of Transformed");
        }

        std::list<gp_Trsf> newTransformations = transFeature->getTransformations(originals);
        if (result.empty()) {
            // First transformation Feature
            result = newTransformations;
            for (auto nt : newTransformations) {
                cogs.push_back(cog.Transformed(nt));
            }
        }
        else {
            // Retain a copy of the first set of transformations for iterator ot
            // We can't iterate through result if we are also adding elements with push_back()!
            std::list<gp_Trsf> oldTransformations;
            result.swap(oldTransformations);  // empty result to receive new transformations
            std::list<gp_Pnt> oldCogs;
            cogs.swap(oldCogs);  // empty cogs to receive new cogs

            if (transFeature->is<PartDesign::Scaled>()) {
                // Diagonal method
                // Multiply every element in the old transformations' slices with the corresponding
                // element in the newTransformations. Example:
                // a11 a12 a13 a14          b1    a11*b1 a12*b1 a13*b1 a14*b1
                // a21 a22 a23 a24   diag   b2  = a21*b2 a22*b2 a23*b2 a24*b1
                // a31 a23 a33 a34          b3    a31*b3 a23*b3 a33*b3 a34*b1
                // In other words, the length of the result vector is equal to the length of the
                // oldTransformations vector

                if (newTransformations.empty()) {
                    throw Base::ValueError("Number of occurrences must be a divisor of previous "
                                           "number of occurrences");
                }
                if (oldTransformations.size() % newTransformations.size() != 0) {
                    throw Base::ValueError("Number of occurrences must be a divisor of previous "
                                           "number of occurrences");
                }

                unsigned sliceLength = oldTransformations.size() / newTransformations.size();
                auto ot = oldTransformations.begin();
                auto oc = oldCogs.begin();

                for (auto const& nt : newTransformations) {
                    for (unsigned s = 0; s < sliceLength; s++) {
                        gp_Trsf trans;
                        double factor = nt.ScaleFactor();  // extract scale factor

                        if (factor > Precision::Confusion()) {
                            trans.SetScale(*oc, factor);  // recreate the scaled transformation to
                                                          // use the correct COG
                            trans = trans * (*ot);
                            cogs.push_back(*oc);  // Scaling does not affect the COG
                        }
                        else {
                            trans = nt * (*ot);
                            cogs.push_back(oc->Transformed(nt));
                        }
                        result.push_back(trans);
                        ++ot;
                        ++oc;
                    }
                }
            }
            else {
                // Multiplication method: Combine the new transformations with the old ones.
                // All old transformations are multiplied with all new ones, so that the length of
                // the result vector is the length of the old and new transformations multiplied.
                // a11 a12         b1    a11*b1 a12*b1 a11*b2 a12*b2 a11*b3 a12*b3
                // a21 a22   mul   b2  = a21*b1 a22*b1 a21*b2 a22*b2 a21*b3 a22*b3
                //                 b3
                for (auto const& nt : newTransformations) {
                    auto oc = oldCogs.begin();

                    for (auto const& ot : oldTransformations) {
                        result.push_back(nt * ot);
                        cogs.push_back(oc->Transformed(nt));
                        ++oc;
                    }
                }
            }
            // What about the Additive method: Take the last (set of) transformations and use them
            // as "originals" for the next transformationFeature, so that something similar to a
            // sweep for transformations could be put together?
        }
    }

    return result;
}

}  // namespace PartDesign
