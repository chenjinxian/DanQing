// SPDX-License-Identifier: LGPL-2.1-or-later
/****************************************************************************
 *                                                                          *
 *   Copyright (c) 2024 The FreeCAD Project Association AISBL               *
 *                                                                          *
 *   This file is part of FreeCAD.                                          *
 *                                                                          *
 *   FreeCAD is free software: you can redistribute it and/or modify it     *
 *   under the terms of the GNU Lesser General Public License as            *
 *   published by the Free Software Foundation, either version 2.1 of the   *
 *   License, or (at your option) any later version.                        *
 *                                                                          *
 *   FreeCAD is distributed in the hope that it will be useful, but         *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of             *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU       *
 *   Lesser General Public License for more details.                        *
 *                                                                          *
 *   You should have received a copy of the GNU Lesser General Public       *
 *   License along with FreeCAD. If not, see                                *
 *   <https://www.gnu.org/licenses/>.                                       *
 *                                                                          *
 ***************************************************************************/

// Ported from: FreeCAD src/Mod/Start/Gui/DisplayedFilesModel.cpp
// Simplified for DisplayTestApp UI shell — no file scanning, empty model only
#include "DisplayedFilesModel.h"

using namespace Start;

DisplayedFilesModel::DisplayedFilesModel(QObject* parent)
    : QAbstractListModel(parent)
{}

int DisplayedFilesModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return 0;
}

QVariant DisplayedFilesModel::data(const QModelIndex& index, int role) const
{
    Q_UNUSED(index); Q_UNUSED(role);
    return {};
}

void DisplayedFilesModel::addFile(const QString& filePath)
{
    Q_UNUSED(filePath);
}

void DisplayedFilesModel::clear()
{
}

QHash<int, QByteArray> DisplayedFilesModel::roleNames() const
{
    return {};
}

