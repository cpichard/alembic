//-*****************************************************************************
//
// Copyright (c) 2009-2010,
//  Sony Pictures Imageworks, Inc. and
//  Industrial Light & Magic, a division of Lucasfilm Entertainment Company Ltd.
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// *       Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// *       Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
// *       Neither the name of Sony Pictures Imageworks, nor
// Industrial Light & Magic nor the names of their contributors may be used
// to endorse or promote products derived from this software without specific
// prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//-*****************************************************************************

#include <Alembic/AbcGeom/OSubD.h>
#include <Alembic/AbcGeom/GeometryScope.h>

namespace Alembic {
namespace AbcGeom {

//-*****************************************************************************
inline void SetPropUsePrevIfNull( Abc::OInt32Property iProp, int32_t iVal )
{
    if ( iVal != ABC_GEOM_SUBD_NULL_INT_VALUE )
    {
        iProp.set( iVal );
    }
    else
    {
        iProp.setFromPrevious( );
    }
}

//-*****************************************************************************
void OSubDSchema::set( const Sample &iSamp )
{
    ALEMBIC_ABC_SAFE_CALL_BEGIN( "OSubDSchema::set()" );

    // We could add sample integrity checking here.
    if ( m_positions.getNumSamples() == 0 )
    {
        // First sample must be valid on all points.
        ABCA_ASSERT( iSamp.getPositions() &&
                     iSamp.getFaceIndices() &&
                     iSamp.getFaceCounts(),
                     "Sample 0 must have valid data for all mesh components" );

        m_positions.set( iSamp.getPositions() );
        m_faceIndices.set( iSamp.getFaceIndices() );
        m_faceCounts.set( iSamp.getFaceCounts() );

        // do we need to create child bounds?
        if ( iSamp.getChildBounds().hasVolume() && !m_childBounds)
        {
            m_childBounds = Abc::OBox3dProperty( *this, ".childBnds", 
                m_positions.getTimeSampling() );
            Abc::Box3d emptyBox;
            emptyBox.makeEmpty();

            // -1 because we just dis an m_positions set above
            size_t numSamples = m_positions.getNumSamples() - 1;

            // set all the missing samples
            for ( size_t i = 0; i < numSamples; ++i )
            {
                m_childBounds.set( emptyBox );
            }
        }

        if (m_childBounds)
        { m_childBounds.set( iSamp.getChildBounds() ); }

        if ( iSamp.getSelfBounds().isEmpty() )
        {
            // OTypedScalarProperty::set() is not referentially transparent,
            // so we need a a placeholder variable.
            Abc::Box3d bnds(
                ComputeBoundsFromPositions( iSamp.getPositions() )
                           );
            m_selfBounds.set( bnds );
        }
        else 
        {
            m_selfBounds.set( iSamp.getSelfBounds() );
        }

        if ( iSamp.getUVs().getVals() )
        {
            if ( iSamp.getUVs().getIndices() )
            {
                // UVs are indexed
                m_uvs = OV2fGeomParam( *this, "uv", true,
                                       iSamp.getUVs().getScope(), 1,
                                       this->getTimeSampling() );
            }
            else
            {
                // UVs are not indexed
                m_uvs = OV2fGeomParam( *this, "uv", false,
                                       iSamp.getUVs().getScope(), 1,
                                       this->getTimeSampling() );
            }

            m_uvs.set( iSamp.getUVs() );
        }

        if ( iSamp.getFaceVaryingInterpolateBoundary() ==
             ABC_GEOM_SUBD_NULL_INT_VALUE )
        {
            m_faceVaryingInterpolateBoundary.set( 0 );
        }
        else
        {
            m_faceVaryingInterpolateBoundary.set(
                iSamp.getFaceVaryingInterpolateBoundary() );
        }
        if ( iSamp.getFaceVaryingPropagateCorners() ==
             ABC_GEOM_SUBD_NULL_INT_VALUE )
        {
            m_faceVaryingPropagateCorners.set( 0 );
        }
        else
        {
            m_faceVaryingPropagateCorners.set(
                iSamp.getFaceVaryingPropagateCorners() );
        }
        if ( iSamp.getInterpolateBoundary() ==
             ABC_GEOM_SUBD_NULL_INT_VALUE )
        {
            m_interpolateBoundary.set( 0 );
        }
        else
        {
            m_interpolateBoundary.set( iSamp.getInterpolateBoundary() );
        }

        m_subdScheme.set( iSamp.getSubdivisionScheme() );

        if ( iSamp.getCreaseIndices() || iSamp.getCreaseLengths() ||
            iSamp.getCreaseSharpnesses() )
        {
            initCreases(0);
        }

        if ( iSamp.getCreaseIndices() )
        {
            m_creaseIndices.set( iSamp.getCreaseIndices() );
        }

        if ( iSamp.getCreaseLengths() )
        {
            m_creaseLengths.set( iSamp.getCreaseLengths() );
        }

        if ( iSamp.getCreaseSharpnesses() )
        {
            m_creaseSharpnesses.set( iSamp.getCreaseSharpnesses() );
        }

        if ( iSamp.getCornerIndices() || iSamp.getCornerSharpnesses() )
        {
            initCorners(0);
        }

        if ( iSamp.getCornerIndices() )
        {
            m_cornerIndices.set( iSamp.getCornerIndices() );
        }

        if ( iSamp.getCornerSharpnesses() )
        {
            m_cornerSharpnesses.set( iSamp.getCornerSharpnesses() );
        }

        if ( iSamp.getHoles() )
        {
            initHoles(0);
        }

        if ( iSamp.getHoles() )
        {
            m_holes.set( iSamp.getHoles() );
        }

    }
    else
    {
        SetPropUsePrevIfNull( m_positions, iSamp.getPositions() );
        SetPropUsePrevIfNull( m_faceIndices, iSamp.getFaceIndices() );
        SetPropUsePrevIfNull( m_faceCounts, iSamp.getFaceCounts() );

        SetPropUsePrevIfNull( m_faceVaryingInterpolateBoundary,
                              iSamp.getFaceVaryingInterpolateBoundary() );
        SetPropUsePrevIfNull( m_faceVaryingPropagateCorners,
                              iSamp.getFaceVaryingPropagateCorners() );
        SetPropUsePrevIfNull( m_interpolateBoundary,
                              iSamp.getInterpolateBoundary() );

        if ( ( iSamp.getCreaseIndices() || iSamp.getCreaseLengths() ||
            iSamp.getCreaseSharpnesses() ) && !m_creaseIndices )
        {
            initCreases(m_positions.getNumSamples() - 1);
        }

        if ( m_creaseIndices )
        {
            SetPropUsePrevIfNull( m_creaseIndices,
                              iSamp.getCreaseIndices() );
            SetPropUsePrevIfNull( m_creaseLengths,
                              iSamp.getCreaseLengths() );
            SetPropUsePrevIfNull( m_creaseSharpnesses,
                              iSamp.getCreaseSharpnesses() );
        }

        if ( ( iSamp.getCornerIndices() || iSamp.getCornerSharpnesses() ) && 
            !m_cornerIndices )
        {
            initCorners(m_positions.getNumSamples() - 1);
        }

        if ( m_cornerIndices )
        {
            SetPropUsePrevIfNull( m_cornerIndices,
                              iSamp.getCornerIndices() );
            SetPropUsePrevIfNull( m_cornerSharpnesses,
                              iSamp.getCornerSharpnesses() );
        }

        if ( iSamp.getHoles() && !m_holes )
        {
            initHoles(m_positions.getNumSamples() - 1);
        }

        if ( m_holes )
        {
            SetPropUsePrevIfNull( m_holes, iSamp.getHoles() );
        }

        SetPropUsePrevIfNull( m_subdScheme, iSamp.getSubdivisionScheme() );

        if ( m_childBounds )
        {
            SetPropUsePrevIfNull( m_childBounds, iSamp.getChildBounds() );
        }

        if ( iSamp.getSelfBounds().hasVolume() )
        {
            m_selfBounds.set( iSamp.getSelfBounds() );
        }
        else if ( iSamp.getPositions() )
        {
            Abc::Box3d bnds(
                ComputeBoundsFromPositions( iSamp.getPositions() )
                           );
            m_selfBounds.set( bnds );
        }
        else
        {
            m_selfBounds.setFromPrevious();
        }

        if ( m_uvs ) { m_uvs.set( iSamp.getUVs() ); }
    }

    ALEMBIC_ABC_SAFE_CALL_END();
}

//-*****************************************************************************
void OSubDSchema::setFromPrevious()
{
    ALEMBIC_ABC_SAFE_CALL_BEGIN( "OSubDSchema::setFromPrevious" );

    m_positions.setFromPrevious();
    m_faceIndices.setFromPrevious();
    m_faceCounts.setFromPrevious();

    m_faceVaryingInterpolateBoundary.setFromPrevious();
    m_faceVaryingPropagateCorners.setFromPrevious();
    m_interpolateBoundary.setFromPrevious();

    if ( m_creaseIndices )
    {
        m_creaseIndices.setFromPrevious();
        m_creaseLengths.setFromPrevious();
        m_creaseSharpnesses.setFromPrevious();
    }

    if ( m_cornerIndices )
    {
        m_cornerIndices.setFromPrevious();
        m_cornerSharpnesses.setFromPrevious();
    }

    if ( m_holes )
    {
        m_holes.setFromPrevious();
    }

    m_subdScheme.setFromPrevious();

    m_selfBounds.setFromPrevious();

    if ( m_childBounds )
        m_childBounds.setFromPrevious();

    if ( m_uvs ) { m_uvs.setFromPrevious(); }

    ALEMBIC_ABC_SAFE_CALL_END();
}

//-*****************************************************************************
Abc::OCompoundProperty OSubDSchema::getArbGeomParams()
{
    ALEMBIC_ABC_SAFE_CALL_BEGIN( "OSubDSchema::getArbGeomParams()" );

    if ( ! m_arbGeomParams )
    {
        m_arbGeomParams = Abc::OCompoundProperty( this->getPtr(),
            ".arbGeomParams" );
    }

    return m_arbGeomParams;

    ALEMBIC_ABC_SAFE_CALL_END();

    Abc::OCompoundProperty ret;
    return ret;
}

//-*****************************************************************************
void OSubDSchema::init( uint32_t iTsIdx )
{
    ALEMBIC_ABC_SAFE_CALL_BEGIN( "OSubDSchema::init()" );

    AbcA::MetaData mdata;
    SetGeometryScope( mdata, kVertexScope );

    AbcA::CompoundPropertyWriterPtr _this = this->getPtr();

    m_positions = Abc::OV3fArrayProperty( _this, "P", mdata, iTsIdx );

    m_faceIndices = Abc::OInt32ArrayProperty( _this, ".faceIndices", iTsIdx );

    m_faceCounts = Abc::OInt32ArrayProperty( _this, ".faceCounts", iTsIdx );

    m_faceVaryingInterpolateBoundary =
        Abc::OInt32Property( _this, ".faceVaryingInterpolateBoundary", iTsIdx );

    m_faceVaryingPropagateCorners =
        Abc::OInt32Property( _this, ".faceVaryingPropagateCorners", iTsIdx );

    m_interpolateBoundary =
        Abc::OInt32Property( _this, ".interpolateBoundary", iTsIdx );

    m_subdScheme = Abc::OStringProperty( _this, ".scheme", iTsIdx );

    m_selfBounds = Abc::OBox3dProperty( _this, ".selfBnds", iTsIdx );

    ALEMBIC_ABC_SAFE_CALL_END_RESET();
}

//-*****************************************************************************
void OSubDSchema::initCreases(uint32_t iNumSamples)
{
    ALEMBIC_ABC_SAFE_CALL_BEGIN( "OSubDSchema::initCreases()" );

    AbcA::CompoundPropertyWriterPtr _this = this->getPtr();

    AbcA::TimeSamplingPtr ts = m_positions.getTimeSampling();

    m_creaseIndices = Abc::OInt32ArrayProperty( _this, ".creaseIndices", ts );

    m_creaseLengths = Abc::OInt32ArrayProperty( _this, ".creaseLengths", ts );

    m_creaseSharpnesses = Abc::OFloatArrayProperty( _this, ".creaseSharpnesses",
        ts );

    std::vector<int32_t> emptyInt;
    std::vector<float32_t> emptyFloat;

    // set the appropriate samples to empty
    for (uint32_t i = 0; i < iNumSamples; ++i)
    {
        m_creaseIndices.set( Abc::Int32ArraySample( emptyInt ) );
        m_creaseLengths.set( Abc::Int32ArraySample( emptyInt ) );
        m_creaseSharpnesses.set( Abc::FloatArraySample( emptyFloat ) );
    }

    ALEMBIC_ABC_SAFE_CALL_END_RESET();
}

//-*****************************************************************************
void OSubDSchema::initCorners(uint32_t iNumSamples)
{
    ALEMBIC_ABC_SAFE_CALL_BEGIN( "OSubDSchema::initCorners()" );

    AbcA::CompoundPropertyWriterPtr _this = this->getPtr();

    AbcA::TimeSamplingPtr ts = m_positions.getTimeSampling();

    m_cornerIndices = Abc::OInt32ArrayProperty( _this, ".cornerIndices", ts );

    m_cornerSharpnesses = Abc::OFloatArrayProperty( _this, ".cornerSharpnesses",
        ts );

    std::vector<int32_t> emptyInt;
    std::vector<float32_t> emptyFloat;

    // set the appropriate samples to empty
    for (uint32_t i = 0; i < iNumSamples; ++i)
    {
        m_cornerIndices.set( Abc::Int32ArraySample( emptyInt ) );
        m_cornerSharpnesses.set( Abc::FloatArraySample( emptyFloat ) );
    }

    ALEMBIC_ABC_SAFE_CALL_END_RESET();
}


//-*****************************************************************************
void OSubDSchema::initHoles(uint32_t iNumSamples)
{
    ALEMBIC_ABC_SAFE_CALL_BEGIN( "OSubDSchema::initHoles()" );

    AbcA::TimeSamplingPtr ts = m_positions.getTimeSampling();

    m_holes = Abc::OInt32ArrayProperty( this->getPtr(), ".holes", ts );

    std::vector<int32_t> emptyInt;

    // set the appropriate samples to empty
    for (uint32_t i = 0; i < iNumSamples; ++i)
    {
        m_holes.set( Abc::Int32ArraySample( emptyInt ) );
    }

    ALEMBIC_ABC_SAFE_CALL_END_RESET();
}

} // End namespace AbcGeom
} // End namespace Alembic