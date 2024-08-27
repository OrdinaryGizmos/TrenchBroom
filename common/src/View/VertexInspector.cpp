/*
 Copyright (C) 2010-2017 Kristian Duske

 This file is part of TrenchBroom.

 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 TrenchBroom is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with TrenchBroom. If not, see <http://www.gnu.org/licenses/>.
 */

#include <memory>
#include <qframe.h>
#include <qnamespace.h> 
#include <qcolordialog.h>
#include <qpushbutton.h>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QVBoxLayout>

#include "Color.h"
#include "View/MapDocument.h"
#include "View/Splitter.h"
#include "View/TabBook.h"
#include "View/VertexInspector.h"
#include "kdl/memory_utils.h"

namespace TrenchBroom::View{

  VertexInspector::VertexInspector(
    std::weak_ptr<MapDocument> document, GLContextManager& contextManager, QWidget* parent)
  : TabBookPage{parent}
  , m_document(std::move(document))
  , m_model(nullptr)
  {
    createGui(contextManager);
  }

  void VertexInspector::createGui(GLContextManager& /* contextManager */ )
  {
    auto frame = new Splitter{Qt::Vertical};
    m_model = new QColorDialog{};
    m_model->setWindowFlags(Qt::Widget);
    m_model->setOptions(QColorDialog::DontUseNativeDialog
                        | QColorDialog::NoButtons
                        | QColorDialog::ShowAlphaChannel);
    
    frame->addWidget(m_model);

    auto applyButton = new QPushButton{"Apply to Selection"};
    
    frame->addWidget(applyButton);

    
    auto* layout = new QVBoxLayout{};
    layout->addWidget(frame);
    
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addStretch();

    setLayout(layout);

    // Connect OnChange and Apply to changing the verts
    connect(
            applyButton,
            &QPushButton::clicked,
            this,
            &VertexInspector::applyColor);
    
    connect(
            m_model,
            &QColorDialog::currentColorChanged,
            this,
            &VertexInspector::applyColor);
      
  }

  void VertexInspector::applyColor(){
    auto document = kdl::mem_lock(m_document);
    
    auto color = m_model->currentColor();
    auto c = new Color(
                       color.red(),
                       color.green(),
                       color.blue(),
                       color.alpha()
                       );
    document->setVertexColors(*c);
  }

} // namespace Trenchbroom
