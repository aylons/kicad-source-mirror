/*
 * This program source code file is part of KICAD, a free EDA CAD application.
 *
 * Copyright (C) 2016 CERN
 * @author Maciej Suminski <maciej.suminski@cern.ch>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you may find one here:
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 * or you may search the http://www.gnu.org website for the version 2 license,
 * or you may write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#ifndef GRAPHICS_IMPORTER_H
#define GRAPHICS_IMPORTER_H

#include "graphics_import_mgr.h"
#include "graphics_import_plugin.h"

#include <eda_text.h>
#include <math/vector2d.h>

#include <list>
#include <memory>
#include <vector>

class EDA_ITEM;

/**
 * @brief Interface that creates objects representing shapes for a given data model.
 */
class GRAPHICS_IMPORTER
{
public:
    GRAPHICS_IMPORTER();

    virtual ~GRAPHICS_IMPORTER()
    {
    }

    /**
     * @brief Sets the import plugin used to obtain shapes from a file.
     */
    void SetPlugin( std::unique_ptr<GRAPHICS_IMPORT_PLUGIN> aPlugin )
    {
        m_plugin = std::move( aPlugin );
    }

    /**
     * @brief Imports shapes stored in a file.
     *
     * It is important to set the plugin type before importing shapes from a file.
     */
    bool Import( const wxString& aFileName );

    /**
     * @brief Sets the line width for the imported outlines.
     */
    void SetLineWidth( unsigned int aWidth )
    {
        m_lineWidth = aWidth;
    }

    /**
     * @brief Returns the line width used for importing the outlines.
     */
    unsigned int GetLineWidth() const
    {
        return m_lineWidth;
    }

    /**
     * @brief Sets scale affecting the imported shapes.
     */
    void SetScale( double aScale )
    {
        m_scale = aScale;
    }

    /**
     * @brief Returns the scale factor affecting the imported shapes.
     */
    double GetScale() const
    {
        return m_scale;
    }

    /**
     * @breif Returns the list of objects representing the imported shapes.
     */
    std::list<std::unique_ptr<EDA_ITEM>>& GetItems()
    {
        return m_items;
    }

    ///> Default line thickness (in internal units)
    static constexpr unsigned int DEFAULT_LINE_WIDTH = 1;

    // Methods to be implemented by derived graphics importers

    /**
     * @brief Creates an object representing a line segment.
     * @param aOrigin is the segment origin point expressed in internal units.
     * @param aEnd is the segment end point expressed in internal units.
     */
    virtual void AddLine( const VECTOR2D& aOrigin, const VECTOR2D& aEnd ) = 0;

    /**
     * @brief Creates an object representing a circle.
     * @param aCenter is the circle center point expressed in internal units.
     * @param aRadius is the circle radius expressed in internal units.
     */
    virtual void AddCircle( const VECTOR2D& aCenter, unsigned int aRadius ) = 0;

    /**
     * @brief Creates an object representing an arc.
     * @param aCenter is the arc center point expressed in internal units.
     * @param aStart is the arc arm end point expressed in internal units.
     * Its length is the arc radius.
     * @param aAgnle is the arc angle expressed in decidegrees.
     */
    virtual void AddArc( const VECTOR2D& aCenter, const VECTOR2D& aStart, double aAngle ) = 0;

    virtual void AddPolygon( const std::vector< VECTOR2D >& aVertices ) = 0;

    //virtual void AddArc( const VECTOR2D& aOrigin, double aStartAngle, double aEndAngle ) = 0;
    //
    /**
     * @brief Creates an object representing a text.
     * @param aOrigin is the text position.
     * @param aText is the displayed text.
     * @param aHeight is the text height expressed in internal units.
     * @param aWidth is the text width expressed in internal units.
     * @param aOrientation is the text orientation angle expressed in decidegrees.
     * @param aHJustify is the text horizontal justification.
     * @param aVJustify is the text vertical justification.
     */
    virtual void AddText( const VECTOR2D& aOrigin, const wxString& aText,
            unsigned int aHeight, unsigned aWidth, double aOrientation,
            EDA_TEXT_HJUSTIFY_T aHJustify, EDA_TEXT_VJUSTIFY_T aVJustify ) = 0;

protected:
    ///> Adds an item to the imported shapes list.
    void addItem( std::unique_ptr<EDA_ITEM> aItem )
    {
        m_items.emplace_back( std::move( aItem ) );
    }

private:
    ///> List of imported items
    std::list<std::unique_ptr<EDA_ITEM>> m_items;

    ///> Plugin used to load a file
    std::unique_ptr<GRAPHICS_IMPORT_PLUGIN> m_plugin;

    ///> Default line thickness for the imported graphics
    unsigned int m_lineWidth;

    ///> Scale factor applied to the imported graphics
    double m_scale;
};

#endif /* GRAPHICS_IMPORTER_H */
