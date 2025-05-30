/***************************************************************************
 *   Copyright (c) 2015 Stefan Tröger <stefantroeger@gmx.net>              *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/

#include "PreCompiled.h"

#ifndef _PreComp_
# include <QMenu>
# include <QMessageBox>
#endif

#include <Gui/BitmapFactory.h>
#include <Gui/Command.h>
#include <Gui/Control.h>
#include <Gui/MainWindow.h>
#include <Gui/TaskView/TaskDialog.h>
#include <Mod/PartDesign/App/FeaturePrimitive.h>

#include "ViewProviderPrimitive.h"
#include "TaskPrimitiveParameters.h"


using namespace PartDesignGui;

PROPERTY_SOURCE(PartDesignGui::ViewProviderPrimitive,PartDesignGui::ViewProvider)

ViewProviderPrimitive::ViewProviderPrimitive() = default;

ViewProviderPrimitive::~ViewProviderPrimitive() = default;

void ViewProviderPrimitive::attach(App::DocumentObject* obj) {
    ViewProviderAddSub::attach(obj);
}

void ViewProviderPrimitive::setupContextMenu(QMenu* menu, QObject* receiver, const char* member)
{
    addDefaultAction(menu, QObject::tr("Edit primitive"));
    PartDesignGui::ViewProvider::setupContextMenu(menu, receiver, member);
}

bool ViewProviderPrimitive::setEdit(int ModNum)
{
    if (ModNum == ViewProvider::Default ) {
        // When double-clicking on the item for this fillet the
        // object unsets and sets its edit mode without closing
        // the task panel
        Gui::TaskView::TaskDialog *dlg = Gui::Control().activeDialog();
        TaskPrimitiveParameters *primitiveDlg = qobject_cast<TaskPrimitiveParameters *>(dlg);
        if (dlg && !primitiveDlg) {
            QMessageBox msgBox(Gui::getMainWindow());
            msgBox.setText(QObject::tr("A dialog is already open in the task panel"));
            msgBox.setInformativeText(QObject::tr("Do you want to close this dialog?"));
            msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
            msgBox.setDefaultButton(QMessageBox::Yes);
            int ret = msgBox.exec();
            if (ret == QMessageBox::Yes)
                Gui::Control().closeDialog();
            else
                return false;
        }

        // clear the selection (convenience)
        Gui::Selection().clearSelection();

        // always change to PartDesign WB, remember where we come from
        oldWb = Gui::Command::assureWorkbench("PartDesignWorkbench");

        // start the edit dialog
        // another pad left open its task panel
        if (primitiveDlg)
            Gui::Control().showDialog(primitiveDlg);
        else
            Gui::Control().showDialog(new TaskPrimitiveParameters(this));

        setPreviewDisplayMode(true);

        return true;
    }
    else {
        return ViewProviderAddSub::setEdit(ModNum);
    }
}

void ViewProviderPrimitive::unsetEdit(int ModNum)
{
    setPreviewDisplayMode(false);

    // Rely on parent class to:
    // restitute old workbench (set setEdit above) and close the dialog if exiting editing
    PartDesignGui::ViewProvider::unsetEdit(ModNum);

}

void ViewProviderPrimitive::updateData(const App::Property* p) {
    PartDesignGui::ViewProviderAddSub::updateData(p);
}

QIcon ViewProviderPrimitive::getIcon() const {

    QString str = QStringLiteral("PartDesign_");
    auto* prim = getObject<PartDesign::FeaturePrimitive>();
    if(prim->getAddSubType() == PartDesign::FeatureAddSub::Additive)
        str += QStringLiteral("Additive");
    else
        str += QStringLiteral("Subtractive");

    switch(prim->getPrimitiveType()) {
    case PartDesign::FeaturePrimitive::Box:
        str += QStringLiteral("Box");
        break;
    case PartDesign::FeaturePrimitive::Cylinder:
        str += QStringLiteral("Cylinder");
        break;
    case PartDesign::FeaturePrimitive::Sphere:
        str += QStringLiteral("Sphere");
        break;
    case PartDesign::FeaturePrimitive::Cone:
        str += QStringLiteral("Cone");
        break;
    case PartDesign::FeaturePrimitive::Ellipsoid:
        str += QStringLiteral("Ellipsoid");
        break;
    case PartDesign::FeaturePrimitive::Torus:
        str += QStringLiteral("Torus");
        break;
    case PartDesign::FeaturePrimitive::Prism:
        str += QStringLiteral("Prism");
        break;
    case PartDesign::FeaturePrimitive::Wedge:
        str += QStringLiteral("Wedge");
        break;
    }

    str += QStringLiteral(".svg");
    return PartDesignGui::ViewProvider::mergeGreyableOverlayIcons(Gui::BitmapFactory().pixmap(str.toStdString().c_str()));
}
