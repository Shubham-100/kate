/*  This file is part of the Kate project.
 *
 *  Copyright (C) 2017 Héctor Mesa Jiménez <hector@lcc.uma.es>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#ifndef KATE_PROJECT_CODE_ANALYSIS_SELECTOR_H
#define KATE_PROJECT_CODE_ANALYSIS_SELECTOR_H

#include <QStandardItemModel>

class KateProjectCodeAnalysisSelector
{
public:

    /**
     * Model attachable to a code analysis tool selector.
     *
     * Each item contains a pointer to a KateProjectCodeAnalysisTool
     * associated to the role Qt::UserRole + 1
     */
    static QStandardItemModel *model(QObject* parent = nullptr);
};

#endif // KATE_PROJECT_CODE_ANALYSIS_SELECTOR_H
