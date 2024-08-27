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

#pragma once

#include "View/TabBook.h"

#include <memory>

class QWidget;
class QColorDialog;

namespace TrenchBroom::View
{
  
class GLContextManager;
class MapDocument;

class VertexInspector : public TabBookPage
{
  Q_OBJECT
private:
  std::weak_ptr<MapDocument> m_document;
  QColorDialog* m_model;

public:
  VertexInspector(
    std::weak_ptr<MapDocument> document,
    GLContextManager& contextManager,
    QWidget* parent = nullptr);  
  ~VertexInspector() override = default;

private:
  void createGui(GLContextManager& contextManager);
  void applyColor();
};

} // namespace TrenchBroom::View
