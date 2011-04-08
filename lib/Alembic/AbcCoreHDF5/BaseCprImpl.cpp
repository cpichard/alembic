//-*****************************************************************************
//
// Copyright (c) 2009-2010,
//  Sony Pictures Imageworks Inc. and
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
// Industrial Light & Magic, nor the names of their contributors may be used
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

#include <Alembic/AbcCoreHDF5/BaseCprImpl.h>
#include <Alembic/AbcCoreHDF5/CprImpl.h>
#include <Alembic/AbcCoreHDF5/AprImpl.h>
#include <Alembic/AbcCoreHDF5/SprImpl.h>
#include <Alembic/AbcCoreHDF5/ReadUtil.h>
#include <Alembic/AbcCoreHDF5/HDF5Util.h>

namespace Alembic {
namespace AbcCoreHDF5 {
namespace ALEMBIC_VERSION_NS {

//-*****************************************************************************
struct CprAttrVisitor
{
    CprAttrVisitor() {}

    std::vector<std::string> properties;

    void createNewProperty( const std::string &iName )
    {
        properties.push_back( iName );
    }
};

//-*****************************************************************************
herr_t CprVisitAllAttrsCB( hid_t iGroup,
                           const char *iAttrName,
                           const H5A_info_t *iAinfo,
                           void *iOpData )
{
    // Get the visitor class. This being NULL is a programmer error
    CprAttrVisitor *visitor = ( CprAttrVisitor * )iOpData;
    assert( visitor != NULL );

    if ( iAttrName == NULL || iAttrName[0] == 0 )
    {
        return 0;
    }

    // We only care about the attr name.
    // our names are in the form (for header info)
    // $NAME.type       Property Type, Data Type, Name
    // $NAME.meta       Meta Data
    // $NAME.tspc       Time Sampling Num Samples Per Cycle
    // $NAME.ttpc       Time Sampling Time Per Cycle
    // $NAME.nums       Total number of samples and number of unique samples
    //                  (only if they are different)
    // $NAME.smp0       First sample for smaller properties.
    //                  Larger properties store it in an HDF5 dataset.
    // $NAME.dims       Dimension hint for strings and complex multi-dimensional
    //                  data sets.
    // $NAME.sclr       Hint on only array properties about whether it is scalar
    //                  like, meaning every sample only has 1 DataType in it.
    // we ignore it everything else
    std::string attrName( iAttrName );
    size_t attrNameLen = attrName.size();
    if ( attrNameLen < 6 )
    {
        // An error
        ABCA_THROW( "Invalid attribute in compound property group: "
                    << attrName
                    << std::endl
                    << "... has no suffix" );
        
        // Continue iterating.
        // return 0;
    }

    // Last 5 characters.
    std::string suffix( attrName, attrNameLen-5 );
    if ( suffix == ".type" )
    {
        std::string propertyName( attrName, 0, attrNameLen-5 );
        visitor->createNewProperty( propertyName );
    }

    return 0;
}

//-*****************************************************************************
//-*****************************************************************************
// CLASS
//-*****************************************************************************
//-*****************************************************************************

//-*****************************************************************************
BaseCprImpl::BaseCprImpl( hid_t iParentGroup,
                          const std::string &iName )
  : m_group( -1 )
{
    ABCA_ASSERT( iParentGroup >= 0, "invalid parent group" );

    // Object ptr is left NULL.
    // CprImpl will set it explicitly, and TopCprImpl leaves it NULL.

    // If our group exists, open it. If it does not, this is not a problem!
    // It just means we don't have any subproperties. We essentially are just
    // some floating metadata, at that point.
    if ( !GroupExists( iParentGroup, iName ) )
    {
        // No group. It's okay!
        // Just return - it means we have no subprops!
        return;
    }

    // Okay now open the group.
    m_group = H5Gopen2( iParentGroup, iName.c_str(), H5P_DEFAULT );
    ABCA_ASSERT( m_group >= 0,
                 "Could not open compound property group named: "
                 << iName << ", H5Gopen2 failed" );
    
    // Do the visiting to gather property names.
    CprAttrVisitor visitor;
    try
    {
        herr_t status = H5Aiterate2( m_group,
                                     H5_INDEX_CRT_ORDER,
                                     H5_ITER_INC,
                                     NULL,
                                     CprVisitAllAttrsCB,
                                     ( void * )&visitor );

        ABCA_ASSERT( status >= 0,
                     "CprImpl::init(): H5Aiterate failed" );
    }
    catch ( std::exception & exc )
    {
        ABCA_THROW( "Could not attr iterate property group named: "
                    << iName << ", reason: " << exc.what() );
    }
    catch ( ... )   
    {
        ABCA_THROW( "Could not attr iterate property group named: "
                    << iName << ", unknown reason" );
    }

    size_t index = 0;
    m_propertyHeaders.resize(visitor.properties.size());

    // For each property name, read the various pieces of information
    for ( std::vector<std::string>::iterator siter = visitor.properties.begin();
          siter != visitor.properties.end(); ++siter, ++index )
    {
        m_subProperties[(*siter)] = index;
        m_propertyHeaders[index].name = (*siter);
    }
}

//-*****************************************************************************
// Destructor is at the end, so that this file has a logical ordering that
// matches the order of operations (create, get children, destroy)
//-*****************************************************************************

//-*****************************************************************************
AbcA::ObjectReaderPtr BaseCprImpl::getObject()
{
    ABCA_ASSERT( m_object, "Invalid object in BaseCprImpl::getObject()" );
    return m_object;
}

//-*****************************************************************************
size_t BaseCprImpl::getNumProperties()
{
    return m_propertyHeaders.size();
}

//-*****************************************************************************
const AbcA::PropertyHeader &BaseCprImpl::getPropertyHeader( size_t i )
{
    if ( i < 0 || i > m_propertyHeaders.size() )
    {
        ABCA_THROW( "Out of range index in "
                    << "CprImpl::getPropertyHeader: " << i );
    }

    // read the property header stuff if we haven't yet
    if (m_propertyHeaders[i].header == NULL)
    {
        PropertyHeaderPtr iPtr( new AbcA::PropertyHeader() );
        ReadPropertyHeader( m_group, m_propertyHeaders[i].name, *iPtr );

        if ( iPtr->isSimple() )
        {

            // try to get the time sampling index (default to 0 if it doesn't
            // exist) and use that to get the TimeSamplingPtr from the archive
            uint32_t tsid = 0;
            std::string tsidName = m_propertyHeaders[i].name + ".tsid";
            if ( H5Aexists( m_group, tsidName.c_str() ) )
            {
                ReadScalar( m_group, tsidName.c_str(), H5T_STD_U32LE,
                    H5T_NATIVE_UINT32, &tsid );
            }
            AbcA::TimeSamplingPtr tsPtr =
                getObject()->getArchive()->getTimeSampling( tsid );

            iPtr->setTimeSampling(tsPtr);
        }

        m_propertyHeaders[i].header = iPtr;

        // don't need name anymore
        m_propertyHeaders[i].name = "";
    }

    return *(m_propertyHeaders[i].header);
}

//-*****************************************************************************
const AbcA::PropertyHeader *
BaseCprImpl::getPropertyHeader( const std::string &iName )
{
    SubPropertiesMap::iterator fiter = m_subProperties.find( iName );
    if ( fiter == m_subProperties.end() )
    {
        return NULL;
    }

    return &(getPropertyHeader(fiter->second));
}

//-*****************************************************************************
AbcA::ScalarPropertyReaderPtr
BaseCprImpl::getScalarProperty( const std::string &iName )
{
    SubPropertiesMap::iterator fiter = m_subProperties.find( iName );
    if ( fiter == m_subProperties.end() )
    {
        return AbcA::ScalarPropertyReaderPtr();
    }

    // make sure we've read the header
    getPropertyHeader(fiter->second);
    SubProperty & sub = m_propertyHeaders[fiter->second];

    if ( !(sub.header->isScalar()) )
    {
        ABCA_THROW( "Tried to read a scalar property from a non-scalar: "
                    << iName << ", type: "
                    << sub.header->getPropertyType() );
    }

    AbcA::BasePropertyReaderPtr bptr;
    if ( sub.made.expired() )
    {
        // Make a new one.
        bptr.reset( new SprImpl( this->asCompoundPtr(), m_group, sub.header ) );
        sub.made = bptr;
    }
    else
    {
        bptr = sub.made.lock();
    }

    AbcA::ScalarPropertyReaderPtr ret =
        boost::dynamic_pointer_cast<AbcA::ScalarPropertyReader,
        AbcA::BasePropertyReader>( bptr );
    return ret;
}

//-*****************************************************************************
AbcA::ArrayPropertyReaderPtr
BaseCprImpl::getArrayProperty( const std::string &iName )
{
    SubPropertiesMap::iterator fiter = m_subProperties.find( iName );
    if ( fiter == m_subProperties.end() )
    {
        return AbcA::ArrayPropertyReaderPtr();
    }

    // make sure we've read the header
    getPropertyHeader(fiter->second);
    SubProperty & sub = m_propertyHeaders[fiter->second];

    if ( !(sub.header->isArray()) )
    {
        ABCA_THROW( "Tried to read an array property from a non-array: "
                    << iName << ", type: "
                    << sub.header->getPropertyType() );
    }

    AbcA::BasePropertyReaderPtr bptr;
    if ( sub.made.expired() )
    {
        // Make a new one.
        bptr.reset( new AprImpl( this->asCompoundPtr(), m_group, sub.header ) );
        sub.made = bptr;
    }
    else
    {
        bptr = sub.made.lock();
    }

    AbcA::ArrayPropertyReaderPtr ret =
        boost::dynamic_pointer_cast<AbcA::ArrayPropertyReader,
        AbcA::BasePropertyReader>( bptr );
    return ret;
}

//-*****************************************************************************
AbcA::CompoundPropertyReaderPtr
BaseCprImpl::getCompoundProperty( const std::string &iName )
{
    SubPropertiesMap::iterator fiter = m_subProperties.find( iName );
    if ( fiter == m_subProperties.end() )
    {
        return AbcA::CompoundPropertyReaderPtr();
    }

    // make sure we've read the header
    getPropertyHeader(fiter->second);
    SubProperty & sub = m_propertyHeaders[fiter->second];

    if ( !(sub.header->isCompound()) )
    {
        ABCA_THROW( "Tried to read a compound property from a non-compound: "
                    << iName << ", type: "
                    << sub.header->getPropertyType() );
    }

    AbcA::BasePropertyReaderPtr bptr;
    if ( sub.made.expired() )
    {
        // Make a new one.
        bptr.reset( new CprImpl( this->asCompoundPtr(), m_group, sub.header ) );
        sub.made = bptr;
    }
    else
    {
        bptr = sub.made.lock();
    }

    AbcA::CompoundPropertyReaderPtr ret =
        boost::dynamic_pointer_cast<AbcA::CompoundPropertyReader,
        AbcA::BasePropertyReader>( bptr );
    return ret;
}

//-*****************************************************************************
BaseCprImpl::~BaseCprImpl()
{
    if ( m_group >= 0 )
    {
        H5Gclose( m_group );
        m_group = -1;
    }
}

} // End namespace ALEMBIC_VERSION_NS
} // End namespace AbcCoreHDF5
} // End namespace Alembic