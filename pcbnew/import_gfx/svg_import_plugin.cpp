/*
 * This program source code file is part of KICAD, a free EDA CAD application.
 *
 * Copyright (C) 2016 CERN
 * @author Janito V. Ferreira Filho
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

#include "svg_import_plugin.h"

#include <algorithm>
#include <cmath>

#include <wx/gdicmn.h>
#include <math/vector2d.h>

#include "nanosvg.h"

#include "graphics_importer.h"

static VECTOR2D calculateBezierBoundingBoxExtremity( const float* aCurvePoints,
        std::function< const float&( const float&, const float& ) > comparator );
static float calculateBezierSegmentationThreshold( const float* aCurvePoints );
static void segmentBezierCurve( const VECTOR2D& aStart, const VECTOR2D& aEnd, float aOffset,
        float aStep, const float* aCurvePoints, float aSegmentationThreshold,
        std::vector< VECTOR2D >& aGeneratedPoints );
static void createNewBezierCurveSegments( const VECTOR2D& aStart, const VECTOR2D& aMiddle,
        const VECTOR2D& aEnd, float aOffset, float aStep, const float* aCurvePoints,
        float aSegmentationThreshold, std::vector< VECTOR2D >& aGeneratedPoints );
static VECTOR2D getBezierPoint( const float* aCurvePoints, float aStep );
static VECTOR2D getPoint( const float* aPointCoordinates );
static VECTOR2D getPointInLine( const VECTOR2D& aLineStart, const VECTOR2D& aLineEnd,
        float aDistance );
static float distanceFromPointToLine( const VECTOR2D& aPoint, const VECTOR2D& aLineStart,
        const VECTOR2D& aLineEnd );

bool SVG_IMPORT_PLUGIN::Load( const wxString& aFileName )
{
    wxCHECK( m_importer, false );

    struct NSVGimage* image = nsvgParseFromFile( aFileName.c_str(), "px", 96 );

    wxCHECK( image, false );

    for( NSVGshape* shape = image->shapes; shape != NULL; shape = shape->next )
    {
        m_importer->SetLineWidth( shape->strokeWidth );

        for( NSVGpath* path = shape->paths; path != NULL; path = path->next )
            DrawPath( path->pts, path->npts, path->closed );
    }

    nsvgDelete( image );

    return true;
}


void SVG_IMPORT_PLUGIN::DrawPath( const float* aPoints, int aNumPoints, bool aClosedPath )
{
    const int numBezierPoints = aNumPoints & ~0x3;
    const int numLinePoints = aNumPoints & 0x3;

    const int numBezierCoordinates = numBezierPoints * 2;
    const float* linePoints = aPoints + numBezierCoordinates;

    std::vector< VECTOR2D > collectedPathPoints;

    if( numBezierPoints > 0 )
        DrawCubicBezierPath( aPoints, numBezierPoints, collectedPathPoints );

    if( numLinePoints > 0 )
        DrawLinePath( linePoints, numLinePoints, collectedPathPoints );

    if( aClosedPath )
        DrawPolygon( collectedPathPoints );
    else
        DrawLineSegments( collectedPathPoints );
}


void SVG_IMPORT_PLUGIN::DrawCubicBezierPath( const float* aPoints, int aNumPoints,
        std::vector< VECTOR2D >& aGeneratedPoints )
{
    const int pointsPerSegment = 4;
    const int curveSpecificPointsPerSegment = 3;
    const int curveSpecificCoordinatesPerSegment = 2 * curveSpecificPointsPerSegment;
    const float* currentPoints = aPoints;
    int remainingPoints = aNumPoints;

    while( remainingPoints >= pointsPerSegment )
    {
        DrawCubicBezierCurve( currentPoints, aGeneratedPoints );
        currentPoints += curveSpecificCoordinatesPerSegment;
        remainingPoints -= curveSpecificPointsPerSegment;
    }
}


void SVG_IMPORT_PLUGIN::DrawCubicBezierCurve( const float* aPoints,
        std::vector< VECTOR2D >& aGeneratedPoints )
{
    auto start = getBezierPoint( aPoints, 0.f );
    auto end = getBezierPoint( aPoints, 1.f );
    auto segmentationThreshold = calculateBezierSegmentationThreshold( aPoints );

    aGeneratedPoints.push_back( start );
    segmentBezierCurve( start, end, 0.f, 0.5f, aPoints, segmentationThreshold, aGeneratedPoints );
    aGeneratedPoints.push_back( end );
}


void SVG_IMPORT_PLUGIN::DrawLinePath( const float* aPoints, int aNumPoints,
        std::vector< VECTOR2D >& aGeneratedPoints )
{
    const int coordinatesPerPoint = 2;
    const float* remainingPoints = aPoints;
    for( int numPoints = aNumPoints; numPoints > 0; --numPoints )
    {
        auto point = getPoint( remainingPoints );

        aGeneratedPoints.push_back( point );

        remainingPoints+= coordinatesPerPoint;
    }
}


void SVG_IMPORT_PLUGIN::DrawPolygon( const std::vector< VECTOR2D >& aPoints )
{
    m_importer->AddPolygon( aPoints );
}


void SVG_IMPORT_PLUGIN::DrawLineSegments( const std::vector< VECTOR2D >& aPoints )
{
    unsigned int numLineStartPoints = aPoints.size() - 1;

    for( unsigned int pointIndex = 0; pointIndex < numLineStartPoints; ++pointIndex )
        m_importer->AddLine( aPoints[ pointIndex ], aPoints[ pointIndex + 1 ] );
}


static VECTOR2D getPoint( const float* aPointCoordinates )
{
    return VECTOR2D( aPointCoordinates[0], aPointCoordinates[1] );
}


static VECTOR2D getBezierPoint( const float* aPoints, float aStep )
{
    const int coordinatesPerPoint = 2;

    auto firstCubicPoint = getPoint( aPoints );
    auto secondCubicPoint = getPoint( aPoints + 1 * coordinatesPerPoint );
    auto thirdCubicPoint = getPoint( aPoints + 2 * coordinatesPerPoint );
    auto fourthCubicPoint = getPoint( aPoints + 3 * coordinatesPerPoint );

    auto firstQuadraticPoint = getPointInLine( firstCubicPoint, secondCubicPoint, aStep );
    auto secondQuadraticPoint = getPointInLine( secondCubicPoint, thirdCubicPoint, aStep );
    auto thirdQuadraticPoint = getPointInLine( thirdCubicPoint, fourthCubicPoint, aStep );

    auto firstLinearPoint = getPointInLine( firstQuadraticPoint, secondQuadraticPoint, aStep );
    auto secondLinearPoint = getPointInLine( secondQuadraticPoint, thirdQuadraticPoint, aStep );

    return getPointInLine( firstLinearPoint, secondLinearPoint, aStep );
}


static VECTOR2D getPointInLine( const VECTOR2D& aLineStart, const VECTOR2D& aLineEnd,
        float aDistance )
{
    return aLineStart + ( aLineEnd - aLineStart ) * aDistance;
}


static float calculateBezierSegmentationThreshold( const float* aCurvePoints )
{
    using comparatorFunction = const float&(*)( const float&, const float& );

    auto minimumComparator = static_cast< comparatorFunction >( &std::min );
    auto maximumComparator = static_cast< comparatorFunction >( &std::max );

    VECTOR2D minimum = calculateBezierBoundingBoxExtremity( aCurvePoints, minimumComparator );
    VECTOR2D maximum = calculateBezierBoundingBoxExtremity( aCurvePoints, maximumComparator );
    VECTOR2D boundingBoxDimensions = maximum - minimum;

    return 0.001 * std::max( boundingBoxDimensions.x, boundingBoxDimensions.y );
}


static VECTOR2D calculateBezierBoundingBoxExtremity( const float* aCurvePoints,
        std::function< const float&( const float&, const float& ) > comparator )
{
    float x, y;

    x = aCurvePoints[0];
    y = aCurvePoints[1];

    for( int pointIndex = 1; pointIndex < 3; ++pointIndex )
    {
        x = comparator( x, aCurvePoints[ 2 * pointIndex ] );
        y = comparator( y, aCurvePoints[ 2 * pointIndex + 1 ] );
    }

    return VECTOR2D( x, y );
}


static void segmentBezierCurve( const VECTOR2D& aStart, const VECTOR2D& aEnd, float aOffset,
        float aStep, const float* aCurvePoints, float aSegmentationThreshold,
        std::vector< VECTOR2D >& aGeneratedPoints )
{
    VECTOR2D middle = getBezierPoint( aCurvePoints, aOffset + aStep );
    float distanceToPreviousSegment = distanceFromPointToLine( middle, aStart, aEnd );

    if( distanceToPreviousSegment > aSegmentationThreshold )
    {
        createNewBezierCurveSegments( aStart, middle, aEnd, aOffset, aStep, aCurvePoints,
                aSegmentationThreshold, aGeneratedPoints );
    }
}


static void createNewBezierCurveSegments( const VECTOR2D& aStart, const VECTOR2D& aMiddle,
        const VECTOR2D& aEnd, float aOffset, float aStep, const float* aCurvePoints,
        float aSegmentationThreshold, std::vector< VECTOR2D >& aGeneratedPoints )
{
    float newStep = aStep / 2.f;
    float offsetAfterMiddle = aOffset + aStep;

    segmentBezierCurve( aStart, aMiddle, aOffset, newStep, aCurvePoints, aSegmentationThreshold,
            aGeneratedPoints );

    aGeneratedPoints.push_back( aMiddle );

    segmentBezierCurve( aMiddle, aEnd, offsetAfterMiddle, newStep, aCurvePoints,
            aSegmentationThreshold, aGeneratedPoints );
}


static float distanceFromPointToLine( const VECTOR2D& aPoint, const VECTOR2D& aLineStart,
        const VECTOR2D& aLineEnd )
{
    auto lineDirection = aLineEnd - aLineStart;
    auto lineNormal = lineDirection.Perpendicular().Resize( 1.f );
    auto lineStartToPoint = aPoint - aLineStart;

    auto distance = lineNormal.Dot( lineStartToPoint );

    return fabs( distance );
}
